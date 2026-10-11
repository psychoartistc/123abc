#ifdef PAGE_ALLOC_USE_BUDDY

#include <arch/paging.h>
#include <limine_requests.h>
#include <mm/common.h>
#include <mm/physical/buddy.h>
#include <utils/errno.h>
#include <utils/fmt_size.h>
#include <utils/logging.h>
#include <utils/panic.h>

static buddy_allocator_t s_allocator = {0};

static uptr reserve_metadata(size_t bytes) {
  struct limine_memmap_response *resp = memmap_request.response;
  struct limine_memmap_entry *best = NULL;

  if (resp == NULL)
    panic("[pmem] missing Limine memory map");

  for (u64 i = 0; i < resp->entry_count; ++i) {
    struct limine_memmap_entry *e = resp->entries[i];

    if (e == NULL || e->type != LIMINE_MEMMAP_USABLE)
      continue;

    if (best == NULL || e->length > best->length)
      best = e;
  }

  if (best == NULL)
    panic("[pmem] no usable memory for buddy metadata");

  uptr base = align_up_ptr(best->base, (uintptr_t)PAGE_SIZE);

  uptr used = base - best->base;

  if ((uptr)bytes > UINTPTR_MAX - used)
    panic("[pmem] buddy metadata size overflow");

  used += (uptr)bytes;

  uptr reserved = align_up_ptr(used, (uintptr_t)PAGE_SIZE);

  if (reserved > best->length) {
    panic("[pmem] not enough RAM for buddy metadata "
          "(need %zu bytes)",
          (size_t)reserved);
  }

  best->base += reserved;
  best->length -= reserved;

  char buff[FMTSIZE_BUFSIZE];
  fmt_size(buff, reserved);
  pr_info("[pmem] reserved %s for buddy metadata", buff);

  return base;
}

static size_t init_levels(void) {
  u64 frames =
      atomic_load_explicit(&s_allocator.frame_count, memory_order_relaxed);

  size_t next_bytes[BUDDY_MAX_ORDER + 1];
  size_t bitmap_bytes[BUDDY_MAX_ORDER + 1];
  size_t total_bytes = 0;

  for (size_t order = 0; order <= BUDDY_MAX_ORDER; ++order) {
    size_t block_count = div_ceil_size((size_t)frames, (size_t)1 << order);

    if (block_count > UINT32_MAX) {
      panic("[pmem] buddy order %zu has too many "
            "blocks (%zu)",
            order, block_count);
    }

    buddy_order_level_t *lvl = &s_allocator.levels[order];

    lvl->block_count = block_count;

    next_bytes[order] =
        align_up_size(block_count * sizeof(u32), BUDDY_CACHE_ALIGN);

    size_t words = div_ceil_size(block_count, 64);

    bitmap_bytes[order] = align_up_size(words * sizeof(u64), BUDDY_CACHE_ALIGN);

    total_bytes += next_bytes[order] + bitmap_bytes[order];
  }

  uptr meta_phys = reserve_metadata(total_bytes);

  // hhdm_queue_map(meta_phys, div_ceil_size(total_bytes, PAGE_SIZE));

  uint8_t *meta = (uint8_t *)p2v(meta_phys);

  if (meta == NULL)
    panic("[pmem] failed to map buddy metadata at %p", (void *)meta_phys);

  size_t offset = 0;

  for (size_t order = 0; order <= BUDDY_MAX_ORDER; ++order) {
    buddy_order_level_t *lvl = &s_allocator.levels[order];

    lvl->next = (u32 *)(meta + offset);
    offset += next_bytes[order];

    lvl->free_bitmap = (u64 *)(meta + offset);
    offset += bitmap_bytes[order];

    for (size_t i = 0; i < lvl->block_count; ++i) {
      atomic_init(&lvl->next[i], BUDDY_NIL_INDEX);
    }

    size_t words = div_ceil_size(lvl->block_count, 64);

    for (size_t i = 0; i < words; ++i) {
      atomic_init(&lvl->free_bitmap[i], 0);
    }

    atomic_init(&lvl->head, pack_head(BUDDY_NIL_INDEX, 0));
  }

  return total_bytes;
}
static bool bitmap_cas_bit(buddy_order_level_t *lvl, u32 idx, bool expected,
                           bool desired) {
  u64 *word = &lvl->free_bitmap[idx / 64];

  u64 mask = UINT64_C(1) << (idx % 64);

  u64 old = atomic_load_explicit(word, memory_order_relaxed);

  for (;;) {
    bool current = (old & mask) != 0;

    if (current != expected)
      return false;

    u64 desired_word = desired ? (old | mask) : (old & ~mask);

    if (atomic_compare_exchange_weak_explicit(word, &old, desired_word,
                                              memory_order_acq_rel,
                                              memory_order_relaxed)) {
      return true;
    }
  }
}

static inline bool bitmap_try_set(buddy_order_level_t *lvl, u32 idx) {
  return bitmap_cas_bit(lvl, idx, false, true);
}

static inline bool bitmap_try_clear(buddy_order_level_t *lvl, u32 idx) {
  return bitmap_cas_bit(lvl, idx, true, false);
}
static void stack_push(buddy_order_level_t *lvl, u32 idx) {
  if (idx >= lvl->block_count || idx == BUDDY_NIL_INDEX) {
    panic("[pmem] stack_push index out of range");
  }

  u64 old_head = atomic_load_explicit(&lvl->head, memory_order_relaxed);

  for (;;) {
    atomic_store_explicit(&lvl->next[idx], head_index(old_head),
                          memory_order_relaxed);

    u64 new_head = pack_head(idx, head_tag(old_head) + 1);

    if (atomic_compare_exchange_weak_explicit(&lvl->head, &old_head, new_head,
                                              memory_order_release,
                                              memory_order_relaxed)) {
      return;
    }
  }
}

static bool stack_pop(buddy_order_level_t *lvl, u32 *out_idx) {
  u64 old_head = atomic_load_explicit(&lvl->head, memory_order_acquire);

  for (;;) {
    u32 idx = head_index(old_head);

    if (idx == BUDDY_NIL_INDEX)
      return false;

    if (idx >= lvl->block_count)
      panic("[pmem] corrupted buddy free-list head");

    u32 next = atomic_load_explicit(&lvl->next[idx], memory_order_relaxed);

    u64 new_head = pack_head(next, head_tag(old_head) + 1);

    if (atomic_compare_exchange_weak_explicit(&lvl->head, &old_head, new_head,
                                              memory_order_acquire,
                                              memory_order_relaxed)) {
      *out_idx = idx;
      return true;
    }
  }
}

static bool alloc_order(size_t order, u32 *out_idx) {
  if (order > BUDDY_MAX_ORDER)
    return false;

  buddy_order_level_t *lvl = &s_allocator.levels[order];

  for (;;) {
    u32 idx;

    if (stack_pop(lvl, &idx)) {

      if (bitmap_try_clear(lvl, idx)) {
        *out_idx = idx;
        return true;
      }

      continue;
    }

    u32 parent;

    if (!alloc_order(order + 1, &parent))
      return false;

    u32 child0 = parent * 2;
    u32 child1 = child0 + 1;

    if (child0 >= lvl->block_count || child1 >= lvl->block_count) {
      panic("[pmem] buddy split produced "
            "invalid child");
    }

    if (!bitmap_try_set(lvl, child1)) {
      panic("[pmem] split child unexpectedly "
            "already free");
    }

    stack_push(lvl, child1);

    *out_idx = child0;
    return true;
  }
}

static void free_order(u32 idx, size_t order) {
  for (;;) {
    if (order > BUDDY_MAX_ORDER)
      panic("[pmem] free_order order out of range");

    buddy_order_level_t *lvl = &s_allocator.levels[order];

    if (idx >= lvl->block_count || idx == BUDDY_NIL_INDEX) {
      panic("[pmem] freeing invalid buddy block");
    }

    if (order >= BUDDY_MAX_ORDER) {
      if (!bitmap_try_set(lvl, idx)) {
        panic("[pmem] double free detected "
              "at max order");
      }

      stack_push(lvl, idx);
      return;
    }

    u32 buddy = idx ^ 1u;

    if (buddy < lvl->block_count && bitmap_try_clear(lvl, buddy)) {
      idx >>= 1;
      ++order;
      continue;
    }

    if (!bitmap_try_set(lvl, idx))
      panic("[pmem] double free detected");

    stack_push(lvl, idx);
    return;
  }
}

static void free_block_range(uptr start, uintptr_t end) {
  if (start > end || (start % PAGE_SIZE) != 0 || (end % PAGE_SIZE) != 0) {
    panic("[pmem] invalid non-page-aligned "
          "free range [%p, %p)",
          (void *)start, (void *)end);
  }

  uptr addr = start;

  while (addr < end) {
    uptr remaining = end - addr;
    size_t order = 0;

    while (order < BUDDY_MAX_ORDER) {
      uptr chunk = (uintptr_t)PAGE_SIZE << (order + 1);

      if (chunk > remaining || (addr % chunk) != 0)
        break;

      ++order;
    }

    free_order((u32)((addr / PAGE_SIZE) >> order), order);

    addr += (uptr)PAGE_SIZE << order;
  }
}
static u64 free_pages_count(void) {
  u64 total = 0;

  for (size_t order = 0; order <= BUDDY_MAX_ORDER; ++order) {
    const buddy_order_level_t *lvl = &s_allocator.levels[order];

    size_t words = div_ceil_size(lvl->block_count, 64);

    for (size_t w = 0; w < words; ++w) {
      u64 bits =
          atomic_load_explicit(&lvl->free_bitmap[w], memory_order_relaxed);

      total += (u64)__builtin_popcountll(bits) << order;
    }
  }

  return total;
}

void pmem_init(void) {
  if (s_allocator.initialized)
    panic("[pmem] pmem_init called twice");

  struct limine_memmap_response *resp = memmap_request.response;

  if (resp == NULL)
    panic("[pmem] missing Limine memory map");

  uptr highest = 0;

  for (u64 i = 0; i < resp->entry_count; ++i) {
    struct limine_memmap_entry *e = resp->entries[i];

    if (e == NULL)
      continue;

    if (e->type != LIMINE_MEMMAP_USABLE &&
        e->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE &&
        e->type != LIMINE_MEMMAP_ACPI_RECLAIMABLE)
      continue;

    uptr end = e->base + e->length;

    if (end > highest)
      highest = end;
  }

  highest = align_up_ptr(highest, (uptr)PAGE_SIZE << BUDDY_MAX_ORDER);

  atomic_init(&s_allocator.frame_count, highest / PAGE_SIZE);

  size_t metadata_size = init_levels();

  for (u64 i = 0; i < resp->entry_count; ++i) {
    struct limine_memmap_entry *e = resp->entries[i];

    if (e == NULL || e->type != LIMINE_MEMMAP_USABLE || e->length == 0)
      continue;

    free_block_range(e->base, e->base + e->length);
  }

  s_allocator.initialized = true;

  char mtSizeBuf[FMTSIZE_BUFSIZE], avMemoryBuf[FMTSIZE_BUFSIZE];
  fmt_size(mtSizeBuf, metadata_size);
  fmt_size(avMemoryBuf, free_pages_count() * PAGE_SIZE);
  pr_info("[pmem] initialized, "
          "metadata size: %s, "
          "available memory: %s, "
          "allocator type: %s",
          mtSizeBuf, avMemoryBuf, page_allocator_type());
}

pmalloc_res_t pmalloc(u64 count) {
  pmalloc_res_t result = {
      .addr = 0,
      .error = 0,
      .ok = false,
  };

  if (!s_allocator.initialized)
    panic("[pmem] pmalloc called before pmem_init");

  if (count == 0) {
    result.error = -EINVAL;
    return result;
  }

  size_t order = order_for_count(count);

  if (order > BUDDY_MAX_ORDER) {
    pr_error("[pmem] tried to allocate "
             "%llu contiguous pages, which exceeds "
             "the max buddy order (%zu pages)",
             (unsigned long long)count, (size_t)1 << BUDDY_MAX_ORDER);

    result.error = -EINVAL;
    return result;
  }

  u32 block;

  if (!alloc_order(order, &block)) {
    result.error = -ENOMEM;
    return result;
  }

  uptr pfn = (uintptr_t)block << order;

  result.addr = pfn * PAGE_SIZE;

  result.ok = true;
  return result;
}

void pfree(uptr at, u64 count) {
  if (!s_allocator.initialized)
    panic("[pmem] pfree called before pmem_init");

  if (count == 0)
    return;

  size_t order = order_for_count(count);

  if (order > BUDDY_MAX_ORDER) {
    pr_error("[pmem] tried to free "
             "%llu pages, which exceeds the "
             "max buddy order (%zu pages)",
             (unsigned long long)count, (size_t)1 << BUDDY_MAX_ORDER);
    return;
  }

  if ((at % PAGE_SIZE) != 0) {
    panic("[physical allocator] freed address is "
          "not page aligned: %p",
          (void *)at);
  }

  uptr pfn = at / PAGE_SIZE;

  if ((pfn & (((uptr)1 << order) - 1)) != 0) {
    panic("[pmem] freed address is "
          "not aligned to its order: %p",
          (void *)at);
  }

  free_order((u32)(pfn >> order), order);
}

u64 pmem_free_frames_count(void) {
  if (!s_allocator.initialized)
    return 0;

  return free_pages_count();
}

u64 pmem_total_frames_count(void) {
  return atomic_load_explicit(&s_allocator.frame_count, memory_order_relaxed);
}

void pmem_reclaim_acpi(void) {
  struct limine_memmap_response *resp = memmap_request.response;

  if (resp == NULL)
    panic("[pmem] missing Limine memory map");

  for (u64 i = 0; i < resp->entry_count; ++i) {
    struct limine_memmap_entry *e = resp->entries[i];

    if (e == NULL || e->type != LIMINE_MEMMAP_ACPI_RECLAIMABLE)
      continue;

    free_block_range(e->base, e->base + e->length);
  }
}

void pmem_reclaim_bootloader(void) {
  struct limine_memmap_response *resp = memmap_request.response;

  if (resp == NULL)
    panic("[pmem] missing Limine memory map");

  for (u64 i = 0; i < resp->entry_count; ++i) {
    struct limine_memmap_entry *e = resp->entries[i];

    if (e == NULL || e->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE)
      continue;

    free_block_range(e->base, e->base + e->length);
  }
}

#endif

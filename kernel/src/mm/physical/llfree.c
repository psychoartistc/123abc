#ifdef PAGE_ALLOC_USE_LLFREE

#include <arch/paging.h>
#include <mm/common.h>
#include <mm/physical/allocator.h>
#include <mm/physical/llfree/llfree.h>
#include <mm/physical/llfree/llfree_inner.h>
#include <scheduler/smp/cpus.h>
#include <stdatomic.h>
#include <utils/fmt_size.h>
#include <utils/logging.h>

static llfree_t s_state = {};
static llfree_classing_t s_classing = {};
static size_t s_frameCount = 0;

static inline size_t pfn_of(uintptr_t phys) { return phys / PAGE_SIZE; }

static inline llfree_request_t llfree_do_request(uint8_t order, size_t core) {
  return llfree_simple_request(cpus_count(), order, core);
}

uintptr_t reserve_metadata(size_t bytes) {
  struct limine_memmap_entry *best = NULL;
  for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
    struct limine_memmap_entry *e = memmap_request.response->entries[i];

    if (e->type != LIMINE_MEMMAP_USABLE)
      continue;

    if (best == NULL || e->length > best->length)
      best = e;
  }

  assert(best != NULL &&
         "[page_allocator] not enough RAM for llfree metadata !");

  uintptr_t base = align_up(best->base, LLFREE_CACHE_SIZE);

  uintptr_t used = (base - best->base) + bytes;

  assert(used <= best->length &&
         "[page_allocator] not enough ram for llfree metadata !");

  best->base += used;
  best->length -= used;

  return base;
}

void free_range(uintptr_t start, uintptr_t end) {
  uintptr_t addr = start;

  while (addr < end) {
    uintptr_t remaining = end - addr;
    uint8_t order = 0;

    while (order < LLFREE_MAX_ORDER) {
      uintptr_t chunk = (uintptr_t)PAGE_SIZE << (order + 1);
      if (chunk > remaining || (addr % chunk) != 0)
        break;

      order++;
    }

    frame_id_t pfn = {.value = pfn_of(addr)};
    llfree_result_t res =
        llfree_put(&s_state, pfn, llfree_do_request(order, current_cpu()->idx));
    assert(llfree_is_ok(res));

    addr += (uintptr_t)PAGE_SIZE << order;
  }
}

void pmem_init() {
  uintptr_t highest = 0;
  for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
    struct limine_memmap_entry *e = memmap_request.response->entries[i];

    uintptr_t end = e->base + e->length;
    if (end > highest)
      highest = end;
  }

  highest = align_up(highest, LLFREE_ALIGN);

  atomic_store(&s_frameCount, highest / PAGE_SIZE);

  s_classing = llfree_classing_simple(cpus_count());
  llfree_meta_size_t sizes =
      llfree_metadata_size(&s_classing, atomic_load(&s_frameCount));

  size_t local_sz = align_up(sizes.local, LLFREE_CACHE_SIZE);
  size_t trees_sz = align_up(sizes.trees, LLFREE_CACHE_SIZE);
  size_t lower_sz = align_up(sizes.lower, LLFREE_CACHE_SIZE);

  uintptr_t meta_phys = reserve_metadata(local_sz + trees_sz + lower_sz);

  llfree_meta_t meta = {
      .local = (uint8_t *)p2v(meta_phys),
      .trees = (uint8_t *)p2v(meta_phys + local_sz),
      .lower = (uint8_t *)p2v(meta_phys + local_sz + trees_sz),
  };

  llfree_result_t res = llfree_init(&s_state, atomic_load(&s_frameCount),
                                    LLFREE_INIT_ALLOC, meta, &s_classing);
  assert(llfree_is_ok(res));

  for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
    struct limine_memmap_entry *e = memmap_request.response->entries[i];
    if (e->type != LIMINE_MEMMAP_USABLE || e->length == 0)
      continue;
    free_range(e->base, e->base + e->length);
  }

  char buffer[FMTSIZE_BUFSIZE];
  char buffer2[FMTSIZE_BUFSIZE];
  fmt_size(buffer, local_sz + trees_sz);
  fmt_size(buffer2, pmem_free_frames_count() * PAGE_SIZE);

  pr_info("[pmem] initialized, metadata size: %s, available "
          "memory: %s, "
          "allocator type: %s",
          buffer, buffer2, page_allocator_type());
}

pmalloc_res_t pmalloc(uint64_t c) {
  if (c == 0)
    return (pmalloc_res_t){.addr = 0x0, .ok = false};

  size_t core = current_cpu()->idx;
  size_t order = (c > 1) ? LLFREE_HUGE_ORDER : 0;
  if (unlikely(order == LLFREE_HUGE_ORDER && c > 512)) {
    pr_error("tried to allocate %zu continuous pages but it doesnt fit "
             "in a 2mb page",
             c);
    return (pmalloc_res_t){.addr = 0x0, .ok = false};
  }

  llfree_result_t res =
      llfree_get(&s_state, frame_id_none(), llfree_do_request(order, core));

  if (!llfree_is_ok(res)) {

    return (pmalloc_res_t){.addr = 0x0, .ok = false};
  }

  // m_logger.trace("requested 4K page");
  return (pmalloc_res_t){.addr = res.frame.value * PAGE_SIZE, .ok = true};
}

void pfree(uintptr_t address, uint64_t c) {
  size_t core = current_cpu()->idx;
  size_t order = (c > 1) ? LLFREE_HUGE_ORDER : 0;
  if (order == LLFREE_HUGE_ORDER && c > 512) {
    pr_error("tried to free %zu pages but it doesnt fit in a 2mb page", c);
    return;
  }

  frame_id_t f = {.value = pfn_of(address)};
  llfree_result_t res = llfree_put(&s_state, f, llfree_do_request(order, core));

  if (unlikely(!llfree_is_ok(res))) {
    pr_error("fail to free 0x%p", address);

  } // else
  // m_logger.trace("free 0x%p phys", address);
}

uint64_t pmem_free_frames_count() {
  return llfree_tree_stats(&s_state).free_frames;
}

uint64_t pmem_total_frames_count() { return atomic_load(&s_frameCount); }

#endif

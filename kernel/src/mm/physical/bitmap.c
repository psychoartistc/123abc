#if defined(PAGE_ALLOC_USE_BITMAP)

#include <arch/paging.h>
#include <limine_requests.h>
#include <limits.h>
#include <mm/common.h>
#include <mm/physical/allocator.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <utils/errno.h>
#include <utils/fmt_size.h>
#include <utils/logging.h>
#include <utils/math.h>
#include <utils/memory.h>
#include <utils/panic.h>

typedef struct pm_bitmap {
  u64 *vbase;
  size_t free_count;
  size_t total_count;
  spinlock_t lock;
  bool initialized;
} pm_bitmap_t;

static struct pm_bitmap s_bmp = {0};
static bool s_acpiReclaimed = false;
static bool s_bootloaderReclaimed = false;

static bool is_reclaimable_type(u64 type) {
  return type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
         type == LIMINE_MEMMAP_USABLE ||
         type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE;
}

static size_t entry_page_count(const struct limine_memmap_entry *entry) {
  return div_ceil(entry->length, PAGE_SIZE);
}

static size_t bitmap_word_index(size_t bit) {
  return bit / (sizeof(u64) * CHAR_BIT);
}

static u64 bitmap_bit_mask(size_t bit) {
  return 1ull << (bit % (sizeof(u64) * CHAR_BIT));
}

static bool bitmap_test(size_t bit) {
  return (s_bmp.vbase[bitmap_word_index(bit)] & bitmap_bit_mask(bit)) != 0;
}

static void bitmap_set(size_t bit) {
  s_bmp.vbase[bitmap_word_index(bit)] |= bitmap_bit_mask(bit);
}

static void bitmap_clear(size_t bit) {
  s_bmp.vbase[bitmap_word_index(bit)] &= ~bitmap_bit_mask(bit);
}

static void bitmap_set_range(size_t start, size_t count, bool free) {
  for (size_t i = 0; i < count; i++) {
    if (free)
      bitmap_set(start + i);
    else
      bitmap_clear(start + i);
  }
}

static size_t bitmap_index_for_address(uintptr_t address) {
  struct limine_memmap_response *response = memmap_request.response;
  size_t bit = 0;

  for (size_t i = 0; i < response->entry_count; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    if (!entry)
      continue;

    if (!is_reclaimable_type(entry->type))
      continue;

    size_t pages = entry_page_count(entry);
    uintptr_t end = entry->base + entry->length;

    if (address >= entry->base && address < end)
      return bit + (address - entry->base) / PAGE_SIZE;

    bit += pages;
  }

  return (size_t)-1;
}

static void pmem_reclaim_type(u64 type, bool *reclaimed_flag) {
  struct limine_memmap_response *response = memmap_request.response;
  size_t bit = 0;
  size_t reclaimed = 0;

  spin_lock(&s_bmp.lock);

  if (*reclaimed_flag) {
    spin_unlock(&s_bmp.lock);
    return;
  }

  for (size_t i = 0; i < response->entry_count; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    if (!entry)
      continue;

    if (!is_reclaimable_type(entry->type)) {
      continue;
    }

    size_t pages = entry_page_count(entry);

    if (entry->type == type) {
      bitmap_set_range(bit, pages, true);
      reclaimed += pages;
    }

    bit += pages;
  }

  atomic_fetch_add(&s_bmp.free_count, reclaimed);
  atomic_fetch_add(&s_bmp.total_count, reclaimed);
  *reclaimed_flag = true;

  spin_unlock(&s_bmp.lock);
}

void pmem_init() {
  assert(memmap_request.response && "no memmap, cannot proceed");

  spinlock_init(&s_bmp.lock);

  struct limine_memmap_response *response = memmap_request.response;

  size_t usablePages = 0;
  size_t totalPages = 0;

  for (size_t i = 0; i < response->entry_count; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    if (!entry)
      continue;

    if (!is_reclaimable_type(entry->type))
      continue;

    size_t pages = entry_page_count(entry);

    totalPages += pages;

    if (entry->type == LIMINE_MEMMAP_USABLE)
      usablePages += pages;
  }

  size_t totalBytesNeeded = div_ceil(totalPages, CHAR_BIT);
  size_t bitmapPages = div_ceil(totalBytesNeeded, PAGE_SIZE);
  size_t bitmapStorageBytes = bitmapPages * PAGE_SIZE;

  bool found = false;
  uintptr_t bmpBase = 0;

  for (size_t i = 0; i < response->entry_count; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    if (!entry)
      continue;

    if (entry->type == LIMINE_MEMMAP_USABLE &&
        entry->length >= bitmapStorageBytes) {
      bmpBase = entry->base;
      found = true;
      break;
    }
  }

  assert(found && "no region found large enough for bitmap");
  assert(usablePages >= bitmapPages);

  u64 *vbmpBase = p2v(bmpBase);

  memset(vbmpBase, 0x0, bitmapStorageBytes);

  s_bmp.vbase = vbmpBase;

  size_t bit = 0;

  for (size_t i = 0; i < response->entry_count; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    if (!entry)
      continue;

    if (!is_reclaimable_type(entry->type))
      continue;

    size_t pages = entry_page_count(entry);

    if (entry->type == LIMINE_MEMMAP_USABLE) {
      for (size_t j = 0; j < pages; j++) {
        uintptr_t address = entry->base + j * PAGE_SIZE;

        if (address >= bmpBase && address < bmpBase + bitmapStorageBytes)
          bitmap_clear(bit + j);
        else
          bitmap_set(bit + j);
      }
    }

    bit += pages;
  }

  atomic_store(&s_bmp.total_count, usablePages - bitmapPages);
  atomic_store(&s_bmp.free_count, usablePages - bitmapPages);

  char buffer[FMTSIZE_BUFSIZE] = {0};
  char buffer2[FMTSIZE_BUFSIZE];
  fmt_size(buffer2, bitmapStorageBytes);
  fmt_size(buffer, atomic_load(&s_bmp.free_count) * PAGE_SIZE);
  pr_info("[pmem] loaded page allocator \'%s\', %s physical memory available, "
          "%s metadata size",
          page_allocator_type(), buffer, buffer2);

  s_bmp.initialized = true;
}

pmalloc_res_t pmalloc(u64 count) {
  if (!s_bmp.initialized)
    return (pmalloc_res_t){.addr = 0x0, .error = -EAGAIN, .ok = false};

  if (count == 0)
    return (pmalloc_res_t){.addr = 0x0, .error = 0, .ok = true};

  spin_lock(&s_bmp.lock);

  if (atomic_load(&s_bmp.free_count) < count) {
    spin_unlock(&s_bmp.lock);
    return (pmalloc_res_t){.addr = 0x0, .error = -ENOMEM, .ok = false};
  }

  struct limine_memmap_response *response = memmap_request.response;

  size_t bit = 0;
  size_t run = 0;
  size_t run_start_bit = 0;
  uintptr_t run_start_addr = 0;
  uintptr_t previous_end = 0;

  for (size_t i = 0; i < response->entry_count; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    if (!entry)
      continue;

    if (!is_reclaimable_type(entry->type)) {
      run = 0;
      previous_end = 0;
      continue;
    }

    size_t pages = entry_page_count(entry);

    for (size_t j = 0; j < pages; j++) {
      uintptr_t address = entry->base + j * PAGE_SIZE;
      bool contiguous = run != 0 && address == previous_end;

      if (!contiguous)
        run = 0;

      if (bitmap_test(bit)) {
        if (run == 0) {
          run_start_bit = bit;
          run_start_addr = address;
        }

        run++;

        if (run >= count) {
          bitmap_set_range(run_start_bit, (size_t)count, false);
          atomic_fetch_sub(&s_bmp.free_count, (size_t)count);

          spin_unlock(&s_bmp.lock);

          return (pmalloc_res_t){
              .addr = run_start_addr,
              .error = 0,
              .ok = true,
          };
        }
      } else {
        run = 0;
      }

      previous_end = address + PAGE_SIZE;
      bit++;
    }
  }

  spin_unlock(&s_bmp.lock);

  return (pmalloc_res_t){.addr = 0x0, .error = -ENOMEM, .ok = false};
}

void pfree(uintptr_t at, u64 count) {
  if (count == 0)
    return;

  assert((at % PAGE_SIZE) == 0);

  spin_lock(&s_bmp.lock);

  for (u64 i = 0; i < count; i++) {
    uintptr_t address = at + i * PAGE_SIZE;
    size_t bit = bitmap_index_for_address(address);

    assert(bit != (size_t)-1 && "freeing address outside managed memory");
    assert(!bitmap_test(bit) && "double free?");
  }

  for (u64 i = 0; i < count; i++) {
    uintptr_t address = at + i * PAGE_SIZE;
    size_t bit = bitmap_index_for_address(address);

    bitmap_set(bit);
  }

  atomic_fetch_add(&s_bmp.free_count, (size_t)count);

  spin_unlock(&s_bmp.lock);
}

u64 pmem_free_frames_count() { return atomic_load(&s_bmp.free_count); }

u64 pmem_total_frames_count() { return atomic_load(&s_bmp.total_count); }

void pmem_reclaim_acpi() {
  pmem_reclaim_type(LIMINE_MEMMAP_ACPI_RECLAIMABLE, &s_acpiReclaimed);
}

void pmem_reclaim_bootloader() {
  pmem_reclaim_type(LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE,
                    &s_bootloaderReclaimed);
}

#endif

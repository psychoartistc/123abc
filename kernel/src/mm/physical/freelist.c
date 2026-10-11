#ifdef PAGE_ALLOC_USE_FREELIST
#include <arch/paging.h>
#include <limine_requests.h>
#include <mm/common.h>
#include <mm/physical/allocator.h>
#include <utils/errno.h>
#include <utils/math.h>
#include <utils/panic.h>

typedef struct p_freelist {
  uintptr_t head;
  size_t free_count;
  size_t total_count;
  spinlock_t lock;
} p_freelist_t;

static struct p_freelist s_freelist = {0};

static void flist_add_entry(uintptr_t a) {
  atomic_fetch_add_explicit(&s_freelist.free_count, 1, memory_order_release);

  spin_lock(&s_freelist.lock);
  uintptr_t head = s_freelist.head;
  uintptr_t *virt = (uintptr_t *)p2v(a);
  *virt = head;
  s_freelist.head = a;
  spin_unlock(&s_freelist.lock);
}

static pmalloc_res_t flist_pop() {
  if (atomic_load_explicit(&s_freelist.free_count, memory_order_acquire) == 0)
    return (pmalloc_res_t){.addr = 0x0, .error = -ENOMEM, .ok = false};

  spin_lock(&s_freelist.lock);
  uintptr_t head = s_freelist.head;
  uintptr_t *virt = (uintptr_t *)p2v(s_freelist.head);
  s_freelist.head = *virt;
  spin_unlock(&s_freelist.lock);

  atomic_fetch_sub_explicit(&s_freelist.free_count, 1, memory_order_release);
  return (pmalloc_res_t){.addr = head, .error = 0, .ok = true};
}

static void reclaim_type(uint64_t type) {
  struct limine_memmap_response *resp = memmap_request.response;
  for (size_t i = 0; i < resp->entry_count; i++) {
    struct limine_memmap_entry *entry = resp->entries[i];
    if (!entry)
      continue;

    if (entry->type != type)
      continue;

    size_t pages = div_floor(entry->length, PAGE_SIZE);
    if (pages == 0)
      continue;

    for (size_t j = 0; j < pages; j++) {
      uintptr_t phys = entry->base + (j * PAGE_SIZE);
      flist_add_entry(phys);
      atomic_fetch_add_explicit(&s_freelist.total_count, 1,
                                memory_order_release);
    }
  }
}

void pmem_init() {
  if (!memmap_request.response || memmap_request.response->entry_count < 1)
    panic("no valid memmap provided, cannot proceed");

  reclaim_type(LIMINE_MEMMAP_USABLE);
}

pmalloc_res_t pmalloc(u64 count) {
  if (count > 1)
    return (pmalloc_res_t){.addr = 0x0, .error = -ENOSYS, .ok = false};

  return flist_pop();
}

void pfree(uintptr_t at, u64 count) { flist_add_entry(at); }

uint64_t pmem_free_frames_count() {
  return atomic_load_explicit(&s_freelist.free_count, memory_order_acquire);
}

uint64_t pmem_total_frames_count() {
  return atomic_load_explicit(&s_freelist.total_count, memory_order_acquire);
}

void pmem_reclaim_acpi() { reclaim_type(LIMINE_MEMMAP_ACPI_RECLAIMABLE); }
void pmem_reclaim_bootloader() {
  reclaim_type(LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE);
}
#endif

#ifndef PHYS_ALLOCATOR_H_
#define PHYS_ALLOCATOR_H_

#include <scheduler/spinlock.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <utils/int.h>

/*
 * physical/allocator.h
 * page allocator types */

static inline const char *page_allocator_type() {
#if defined(PAGE_ALLOC_USE_LLFREE)
  return "llfree";
#elif defined(PAGE_ALLOC_USE_BITMAP)
  return "bitmap";
#elif defined(PAGE_ALLOC_USE_FREELIST)
  return "freelist";
#elif defined(PAGE_ALLOC_USE_BUDDY)
  return "buddy";
#else
#error                                                                         \
    "no physical page allocator found ! define one of those: PAGE_ALLOC_USE_LLFREE, PAGE_ALLOC_USE_BITMAP, PAGE_ALLOC_USE_BUDDY"
#endif
}

typedef struct pmalloc_res {
  uintptr_t addr;
  long error;
  bool ok;
} pmalloc_res_t;

void pmem_init();

pmalloc_res_t pmalloc(u64 count);
void pfree(uintptr_t at, u64 count);

u64 pmem_free_frames_count();
u64 pmem_total_frames_count();

void pmem_reclaim_acpi();
void pmem_reclaim_bootloader();

#endif

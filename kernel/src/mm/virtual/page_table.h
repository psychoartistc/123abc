#ifndef PAGE_TABLE_H_
#define PAGE_TABLE_H_

/*
 * page_table.h
 * the page table type */

#include <arch/paging.h>
#include <scheduler/spinlock.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct page_table {
  uintptr_t pml4;
  spinlock_t lock;
} page_table_t;

typedef struct {
  uintptr_t start;
  size_t pages;
} flush_batch_t;

page_table_t *kernel_pt_init();
page_table_t *kernel_pt();

int pt_map(page_table_t *pt, uintptr_t virt, uintptr_t phys, int protect);
int pt_map2mb(page_table_t *pt, uintptr_t virt, uintptr_t phys, int protect);
int pt_map1gb(page_table_t *pt, uintptr_t virt, uintptr_t phys, int protect);
int pt_unmap(page_table_t *pt, uintptr_t virt);

int pt_protect(page_table_t *pt, void *ptr, int protect);
int pt_protect_multiple(page_table_t *pt, void *ptr, size_t pages, int protect);

uintptr_t pt_v2p(page_table_t *pt, uintptr_t first);
void pt_switch(page_table_t *pt);

#endif

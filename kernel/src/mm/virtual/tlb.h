#ifndef TLB_H_
#define TLB_H_

#include <arch/paging.h>
#include <mm/virtual/page_table.h>
#include <stddef.h>
#include <stdint.h>

/*
 * tlb.h
 * tlb functions, tlb shootdown */

void on_tlb_shootdown(uintptr_t pml4, uintptr_t virt, size_t pages);

static inline void tlb_flush(page_table_t *pt, uintptr_t virt, size_t pages) {
  for (size_t i = 0; i < pages; i++)
    local_invlpg(virt + i * PAGE_SIZE);
  on_tlb_shootdown(pt->pml4, virt, pages);
}

void tlb_batch_add(flush_batch_t *b, uintptr_t virt, size_t pages);

void tlb_batch_flush(page_table_t *pt, flush_batch_t *b);

#endif

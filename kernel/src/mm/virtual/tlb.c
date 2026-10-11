#include <mm/virtual/tlb.h>
#include <utils/logging.h>

void tlb_batch_add(flush_batch_t *b, uintptr_t virt, size_t pages) {
  if (b->pages == 0) {
    b->start = virt;
    b->pages = pages;
    return;
  }
  b->pages = (virt + pages * PAGE_SIZE - b->start) / PAGE_SIZE;
}

void tlb_batch_flush(page_table_t *pt, flush_batch_t *b) {
  if (b->pages)
    tlb_flush(pt, b->start, b->pages);
}

void on_tlb_shootdown(uintptr_t pml4, uintptr_t virt, size_t pages) {
  (void)pml4;
  (void)virt;
  (void)pages;
}

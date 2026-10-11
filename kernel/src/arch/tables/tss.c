#include <arch/cpu.h>
#include <arch/tables/tss.h>
#include <utils/logging.h>
#include <utils/memory.h>

void tss_init(tss_t *t) {
  if (unlikely(!t))
    return;

  memset(&t->e, 0, sizeof(tss_entry_t));

  t->e.io_map_base = sizeof(tss_entry_t);

  pr_info("[tss] initialized on core %zu", current_cpu()->idx);
}

// sets the ring zero stack
void tss_rzero_stack_set(tss_t *t, u64 s_top) {
  if (unlikely(!t))
    return;

  t->e.rsp[0] = s_top;
}

void tss_set_ist(tss_t *t, u8 ist_idx, u64 s_top) {
  if (unlikely(!t || ist_idx == 0 || ist_idx > TSS_IST_COUNT))
    return;

  t->e.ist[ist_idx - 1] = s_top;
}

void tss_load(tss_t *t, u16 selector) {
  if (unlikely(!t))
    return;

  asm volatile("ltr %0" : : "r"(selector) : "memory");
}

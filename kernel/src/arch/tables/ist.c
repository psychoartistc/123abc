
#include <arch/tables/ist.h>

u64 ist_get_stack_top(ist_t *t, size_t idx) {
  if (unlikely(!t))
    return 0;

  if (idx >= IST_STACK_COUNT)
    return 0;

  return t->stack_tops[idx];
}

void ist_set_stack_top(ist_t *t, size_t idx, u64 top) {
  if (unlikely(!t))
    return;

  t->stack_tops[idx] = top;
}

void ist_install(ist_t *t, tss_t *tss) {
  if (unlikely(!t || !tss))
    return;

  for (u64 i = 0; i < IST_STACK_COUNT; i++)
    tss_set_ist(tss, (u8)(i + 1), ist_get_stack_top(t, i));
}

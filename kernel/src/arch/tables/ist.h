#ifndef IST_H_
#define IST_H_

/*
 * ist.h
 * interrupt stack table */

#include <arch/tables/tss.h>
#include <stddef.h>
#include <utils/int.h>

#define IST_STACK_COUNT 7

typedef struct {
  u64 stack_tops[IST_STACK_COUNT];
} ist_t;

u64 ist_get_stack_top(ist_t *t, size_t idx);
void ist_set_stack_top(ist_t *t, size_t idx, u64 top);
void ist_install(ist_t *t, tss_t *i);

#endif

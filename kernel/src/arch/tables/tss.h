#ifndef TSS_H_
#define TSS_H_

/*
 * tables/tss.h
 * task segment register */

#include <utils/attributes.h>
#include <utils/branching.h>
#include <utils/int.h>

#define TSS_IST_COUNT 7

typedef struct tss_entry {
  u32 reserved0;
  u64 rsp[3];
  u64 reserved1;
  u64 ist[TSS_IST_COUNT];
  u64 reserved2;
  u16 reserved3;

  u16 io_map_base;
} __PACKED tss_entry_t;

typedef struct {
  tss_entry_t e;
} tss_t;

void tss_init(tss_t *t);

// sets the ring zero stack
void tss_rzero_stack_set(tss_t *t, u64 s_top);

void tss_set_ist(tss_t *t, u8 ist_idx, u64 s_top);

void tss_load(tss_t *t, u16 selector);

#endif

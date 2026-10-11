#ifndef HPET_CLOCK_H_
#define HPET_CLOCK_H_

/*
 * drivers/clocks/hpet.h
 * hpet defines*/

#include <stdbool.h>
#include <utils/int.h>

#define HPET_CAP 0x000
#define HPET_GEN_CONFIG 0x010
#define HPET_INT_STATUS 0x020
#define HPET_MAIN_COUNTER 0x0F0

#define HPET_TIMER_CONFIG(n) (0x100 + 0x20 * (n))
#define HPET_TIMER_COMPARE(n) (0x108 + 0x20 * (n))

#define HPET_TIMER_INDEX 0

#define HPET_ENABLE (1ULL << 0)
#define HPET_LEGACY_REPLACE (1ULL << 1)

#define HPET_COUNTER_64 (1ULL << 13)

#define HPET_TN_INT_TYPE (1ULL << 1)
#define HPET_TN_INT_ENABLE (1ULL << 2)
#define HPET_TN_PERIODIC (1ULL << 3)
#define HPET_TN_SIZE_64 (1ULL << 5)
#define HPET_TN_VALUE_SET (1ULL << 6)
#define HPET_TN_MODE_32 (1ULL << 8)
#define HPET_TN_ROUTE_SHIFT 9
#define HPET_TN_ROUTE_MASK (0x1FULL << HPET_TN_ROUTE_SHIFT)
#define HPET_TN_FSB_ENABLE (1ULL << 14)

#define HPET_MIN_LEAD_TICKS 32ULL

#define HPET_ROUTE 2
#define HPET_VECTOR 52

typedef struct {
  volatile u8 *mmio;

  u64 period_fs;
  u32 nr_timers;

  bool use_64bit_compare;
  bool ready;
  bool started;
} hpet_state_t;

#endif

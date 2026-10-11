#include <arch/clock/clocksource.h>
#include <arch/irq/irq_dispatch.h>
#include <drivers/api.h>
#include <drivers/clocks/hpet.h>
#include <hardware/acpi/parse.h>
#include <hardware/ioapic.h>
#include <mm/common.h>
#include <mm/virtual/page_table.h>
#include <stddef.h>
#include <utils/logging.h>

static hpet_state_t s_hpet;

static inline u64 hpet_read64(u32 offset) {
  return *(volatile u64 *)(s_hpet.mmio + offset);
}

static inline void hpet_write64(u32 offset, u64 value) {
  *(volatile u64 *)(s_hpet.mmio + offset) = value;
}

static inline u32 hpet_read32(u32 offset) {
  return *(volatile u32 *)(s_hpet.mmio + offset);
}

static inline void hpet_write32(u32 offset, u32 value) {
  *(volatile u32 *)(s_hpet.mmio + offset) = value;
}

static inline u64 hpet_read_counter(void) {
  if (s_hpet.use_64bit_compare)
    return hpet_read64(HPET_MAIN_COUNTER);

  return hpet_read32(HPET_MAIN_COUNTER);
}

static int hpet_ns_to_ticks(u64 ns, u64 *result) {
  if (!result || !s_hpet.period_fs)
    return -1;

  __uint128_t scaled = (__uint128_t)ns * 1000000ULL;

  scaled = (scaled + s_hpet.period_fs - 1) / s_hpet.period_fs;

  if (scaled < 1)
    scaled = 1;

  scaled += HPET_MIN_LEAD_TICKS;

  if (s_hpet.use_64bit_compare) {
    if (scaled > 0x7FFFFFFFFFFFFFFFULL)
      return -1;
  } else {
    if (scaled > 0x7FFFFFFFULL)
      return -1;
  }

  *result = (u64)scaled;
  return 0;
}

static int hpet_arm_ns(u64 delay_ns) {
  u64 ticks;

  if (hpet_ns_to_ticks(delay_ns, &ticks) < 0) {
    u64 cfg = hpet_read64(HPET_TIMER_CONFIG(HPET_TIMER_INDEX));

    hpet_write64(HPET_TIMER_CONFIG(HPET_TIMER_INDEX),
                 cfg & ~HPET_TN_INT_ENABLE);

    pr_warn("[hpet] deadline is too large for the comparator mode\n");
    return -1;
  }
  u32 tcfg_offset = HPET_TIMER_CONFIG(HPET_TIMER_INDEX);
  u64 tcfg = hpet_read64(tcfg_offset);

  tcfg &= ~HPET_TN_INT_ENABLE;
  hpet_write64(tcfg_offset, tcfg);

  if (s_hpet.use_64bit_compare) {
    u64 now = hpet_read64(HPET_MAIN_COUNTER);
    hpet_write64(HPET_TIMER_COMPARE(HPET_TIMER_INDEX), now + ticks);
  } else {
    u32 now = hpet_read32(HPET_MAIN_COUNTER);
    u32 target = now + (u32)ticks;

    hpet_write32(HPET_TIMER_COMPARE(HPET_TIMER_INDEX), target);
  }

  tcfg &= ~(HPET_TN_INT_TYPE | HPET_TN_PERIODIC | HPET_TN_VALUE_SET |
            HPET_TN_FSB_ENABLE | HPET_TN_INT_ENABLE);

  hpet_write64(tcfg_offset, tcfg | HPET_TN_INT_ENABLE);

  u64 cfg_rb = hpet_read64(tcfg_offset);
  u64 now_rb = hpet_read_counter();

  u64 cmp_rb;
  if (s_hpet.use_64bit_compare) {
    cmp_rb = hpet_read64(HPET_TIMER_COMPARE(HPET_TIMER_INDEX));
  } else {
    cmp_rb = hpet_read32(HPET_TIMER_COMPARE(HPET_TIMER_INDEX));
  }

  pr_info("[hpet] config=0x%llx enabled=%u periodic=%u\n",
          (unsigned long long)cfg_rb, !!(cfg_rb & HPET_TN_INT_ENABLE),
          !!(cfg_rb & HPET_TN_PERIODIC));

  pr_info("[hpet] now=%llu comparator=%llu ticks=%llu\n",
          (unsigned long long)now_rb, (unsigned long long)cmp_rb,
          (unsigned long long)ticks);
  return 0;
}

static bool hpet_available(void *arg) {
  hpet_state_t *state = arg;

  return state && state->ready;
}

static int hpet_start(u64 initial_deadline, void *arg) {
  hpet_state_t *state = arg;

  if (!state || !state->ready)
    return -1;

  u64 cfg = hpet_read64(HPET_GEN_CONFIG);

  cfg &= ~HPET_LEGACY_REPLACE;
  cfg |= HPET_ENABLE;

  hpet_write64(HPET_GEN_CONFIG, cfg);

  state->started = true;

  int r = hpet_arm_ns(initial_deadline);
  if (r < 0)
    state->started = false;

  if (r == 0) {
    pr_info("[hpet] timer started, initial delay %llu ns",
            (unsigned long long)initial_deadline);
  }

  return r;
}

static int hpet_set_next_deadline(u64 deadline, void *arg) {
  hpet_state_t *state = arg;

  if (!state || !state->ready || !state->started)
    return -1;

  return hpet_arm_ns(deadline);
}

static void hpet_irq(interrupt_frame_t *frame, void *arg) {
  (void)frame;

  hpet_state_t *state = arg;

  if (!state || !state->started)
    return;

  csources_on_tick();
}

static int hpet_clock_entry() {
  acpi_parse_res_t *res = acpi_get_parse_res();
  page_table_t *pt = kernel_pt();

  if (!res || !res->hpet_phys) {
    pr_warn("[hpet] acpi did not provide an hpet address");
    return -1;
  }

  uintptr_t virt = (uintptr_t)p2v(res->hpet_phys);

  int r = pt_map(pt, virt, res->hpet_phys, P_READ | P_WRITE | P_NOCACHE);
  if (r < 0) {
    pr_error("[hpet] failed to map registers: %d", r);
    return r;
  }

  s_hpet.mmio = (volatile u8 *)virt;

  u64 cap = hpet_read64(HPET_CAP);

  s_hpet.period_fs = (cap >> 32) & 0xFFFFFFFFULL;
  s_hpet.nr_timers = (u32)((cap >> 8) & 0x1FULL) + 1;

  if (!s_hpet.period_fs) {
    pr_error("[hpet] hardware reported an invalid counter period");
    return -1;
  }

  if (HPET_TIMER_INDEX >= s_hpet.nr_timers) {
    pr_error("[hpet] timer %u is not implemented", (unsigned)HPET_TIMER_INDEX);
    return -1;
  }

  u32 tcfg_offset = HPET_TIMER_CONFIG(HPET_TIMER_INDEX);
  u64 tcfg = hpet_read64(tcfg_offset);

  u32 route_caps = (u32)(tcfg >> 32);

  if (!(route_caps & (1U << HPET_ROUTE))) {
    pr_error("[hpet] route %u is not supported by timer 0",
             (unsigned)HPET_ROUTE);
    return -1;
  }

  bool counter64 = (cap & HPET_COUNTER_64) != 0;
  bool timer64 = (tcfg & HPET_TN_SIZE_64) != 0;

  s_hpet.use_64bit_compare = counter64 && timer64;

  tcfg &= ~(HPET_TN_INT_TYPE | HPET_TN_INT_ENABLE | HPET_TN_PERIODIC |
            HPET_TN_VALUE_SET | HPET_TN_MODE_32 | HPET_TN_ROUTE_MASK |
            HPET_TN_FSB_ENABLE);

  tcfg |= (u64)HPET_ROUTE << HPET_TN_ROUTE_SHIFT;

  if (!s_hpet.use_64bit_compare)
    tcfg |= HPET_TN_MODE_32;

  hpet_write64(tcfg_offset, tcfg);

  irq_register((u8)HPET_VECTOR, hpet_irq, &s_hpet);

  // we are guaranteed there will be atleast one
  // ioapic because of the assertion after madt has been parsed
  // use the first ioapic
  ioapic_t *ioapic = &res->ioapics.data[0];

  r = ioapic_route_irq(ioapic, HPET_ROUTE, (u8)HPET_VECTOR);
  if (r < 0) {
    pr_error("[hpet] failed to configure interrupt routing: %d", r);
    return r;
  }

  s_hpet.ready = true;

  pr_info("[hpet] configuration:");
  pr_info("\tperiod = %llu fs", (unsigned long long)s_hpet.period_fs);
  pr_info("\ttimers = %u", s_hpet.nr_timers);
  pr_info("\tcounter64 = %u", counter64);
  pr_info("\tcomparator64 = %s", (s_hpet.use_64bit_compare) ? "YES" : "NO");
  pr_info("\troute = %u", (unsigned)HPET_ROUTE);
  pr_info("\tirq vector = 0x%x", (unsigned)HPET_VECTOR);

  csources_add_source((clocksource_t){
      .name = "HPET",
      .arg = &s_hpet,
      .available_check = hpet_available,
      .on_start = hpet_start,
      .on_deadline_set = hpet_set_next_deadline,
      .type = CLOCK_SOURCE_HPET,
      .is_local = false,
  });

  u64 cfg = hpet_read64(HPET_TIMER_CONFIG(HPET_TIMER_INDEX));
  u64 cmp = hpet_read64(HPET_TIMER_COMPARE(HPET_TIMER_INDEX));

  pr_info("[hpet] cfg=0x%llx periodic=%u enabled=%u "
          "comparator=%llu counter=%llu\n",
          (unsigned long long)cfg, !!(cfg & HPET_TN_PERIODIC),
          !!(cfg & HPET_TN_INT_ENABLE), (unsigned long long)cmp,
          (unsigned long long)hpet_read_counter());
  return 0;
}

REGISTER_DRIVER(hpet_clock, "hpet clock", "manages the hpet clock", "1.0.0",
                DRVLICENSE_DEFAULT, DRV_INIT_ORDER_CLOCK_SOURCE,
                hpet_clock_entry, NULL);

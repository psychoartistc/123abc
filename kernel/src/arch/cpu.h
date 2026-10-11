#ifndef CPU_H_
#define CPU_H_

#include <arch/msr.h>
#include <arch/tables/ist.h>
#include <limine_requests.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <utils/attributes.h>

#include <arch/tables/gdt.h>
#include <arch/tables/ist.h>
#include <arch/tables/tss.h>

/*
 * cpu.h
 * cpu local struct */

#define MAX_CORES 64

typedef struct cpu {
  tss_t tss;
  gdt_t gdt;
  ist_t ist;
  uintptr_t ist_stack_tops[IST_STACK_COUNT];
  u64 idx;
  u64 lapic_id;
  uintptr_t kern_stack_top;
} cpu_t;

void set_current_cpu(struct cpu *c);
void cpu_init(cpu_t *cpu);
struct cpu *current_cpu();
noreturn void ap_entry(struct limine_mp_info *info);

static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx) {
  __asm__ volatile("cpuid"
                   : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                   : "a"(leaf));
}
static inline void cpuid_ex(uint32_t leaf, uint32_t subleaf, uint32_t *eax,
                            uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
  __asm__ volatile("cpuid"
                   : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                   : "a"(leaf), "c"(subleaf));
}

static inline bool lapic_tsc_deadline_supported(void) {
  static bool cache = false;
  static bool cached = false;

  if (!cached) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);

    cache = (ecx & (1 << 24)) != 0;
    cached = true;
  }

  return cache;
}

static inline bool invariant_tsc_supported(void) {
  static bool cache = false;
  static bool cached = false;

  if (!cached) {
    uint32_t eax, ebx, ecx, edx;

    cpuid_ex(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000007) {
      return false;
    }

    cpuid_ex(0x80000007, 0, &eax, &ebx, &ecx, &edx);

    cache = (edx & (1 << 8)) != 0;
    cached = true;
  }

  return cache;
}

void dump_cpu_features();

#endif

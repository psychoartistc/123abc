#ifndef IOAPIC_H_
#define IOAPIC_H_

/*
 * ioapic.h
 * ioapic interaction and interrupt routing */

#include <scheduler/spinlock.h>
#include <utils/int.h>

#define IOAPIC_REGSEL 0x00
#define IOAPIC_WINDOW 0x10

#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VERSION 0x01

#define IOAPIC_REDIR_BASE 0x10
#define IOAPIC_REDIR_MASK (1U << 16)

#define IOAPIC_MAX_REDIR_COUNT 120U

typedef struct {
  uptr ioapic_phys;
  uptr ioapic_gsi_base;

  volatile u32 *mmio;
  u32 redir_count;
  u8 dest_apic_id;

  spinlock_t lock;
  bool initialized;
} ioapic_t;

int ioapic_init(ioapic_t *ioapic);
int ioapic_route_irq(ioapic_t *ioapic, u32 route, u8 vec);
int ioapic_route_gsi(u32 gsi, u8 vec);

static inline u8 ioapic_read_boot_apic(void) {
  u32 eax = 1;
  u32 ebx, ecx, edx;

  asm volatile("cpuid"
               : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
               :
               : "memory");

  return (u8)(ebx >> 24);
}

#endif

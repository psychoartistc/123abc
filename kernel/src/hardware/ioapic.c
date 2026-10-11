#include <hardware/acpi/parse.h>
#include <hardware/ioapic.h>
#include <mm/common.h>
#include <mm/virtual/page_table.h>
#include <stdatomic.h>
#include <utils/errno.h>
#include <utils/logging.h>
#include <utils/panic.h>

static inline u32 ioapic_read_reg_locked(ioapic_t *ioapic, u8 reg) {
  ioapic->mmio[IOAPIC_REGSEL / sizeof(u32)] = reg;

  atomic_signal_fence(memory_order_seq_cst);

  u32 value = ioapic->mmio[IOAPIC_WINDOW / sizeof(u32)];

  atomic_signal_fence(memory_order_seq_cst);

  return value;
}

static inline void ioapic_write_reg_locked(ioapic_t *ioapic, u8 reg,
                                           u32 value) {
  ioapic->mmio[IOAPIC_REGSEL / sizeof(u32)] = reg;

  atomic_signal_fence(memory_order_seq_cst);

  ioapic->mmio[IOAPIC_WINDOW / sizeof(u32)] = value;

  atomic_signal_fence(memory_order_seq_cst);
}

int ioapic_init(ioapic_t *ioapic) {
  if (unlikely(!ioapic))
    return -EINVAL;

  if (unlikely(!ioapic->ioapic_phys))
    return -ENOENT;

  atomic_init(&ioapic->initialized, false);
  spinlock_init(&ioapic->lock);

  uintptr_t virt = (uintptr_t)p2v(ioapic->ioapic_phys);
  page_table_t *pt = kernel_pt();

  int r = pt_map(pt, virt, ioapic->ioapic_phys, P_READ | P_WRITE | P_NOCACHE);

  if (r < 0) {
    pr_error("[ioapic] failed to map mmio at %p: %d", (void *)virt, r);
    return r;
  }

  ioapic->mmio = (volatile u32 *)virt;

  ioapic->dest_apic_id = ioapic_read_boot_apic();

  spin_lock(&ioapic->lock);

  u32 version = ioapic_read_reg_locked(ioapic, IOAPIC_REG_VERSION);

  if (version == 0xFFFFFFFFU || (version & 0xFFU) == 0) {
    spin_unlock(&ioapic->lock);

    pr_error("[ioapic] invalid version register: 0x%x\n", version);
    return -EIO;
  }

  u32 max_redir = (version >> 16) & 0xFFU;
  u32 count = max_redir + 1;

  if (count > IOAPIC_MAX_REDIR_COUNT) {
    spin_unlock(&ioapic->lock);

    pr_error("[ioapic] unsupported redirection count: %u", count);
    return -EINVAL;
  }

  ioapic->redir_count = count;

  for (u32 i = 0; i < ioapic->redir_count; ++i) {
    u8 low_reg = (u8)(IOAPIC_REDIR_BASE + 2U * i);

    u32 low = ioapic_read_reg_locked(ioapic, low_reg);

    ioapic_write_reg_locked(ioapic, low_reg, low | IOAPIC_REDIR_MASK);
  }

  spin_unlock(&ioapic->lock);

  atomic_store_explicit(&ioapic->initialized, true, memory_order_release);

  pr_info(
      "[ioapic] initialized, phys %p, gsi %llu, "
      "inputs %u, gsi range %llu to %llu, destination apic id %u",
      (void *)ioapic->ioapic_phys, (unsigned long long)ioapic->ioapic_gsi_base,
      ioapic->redir_count, (unsigned long long)ioapic->ioapic_gsi_base,
      (unsigned long long)(ioapic->ioapic_gsi_base + ioapic->redir_count - 1),
      (unsigned)ioapic->dest_apic_id);

  return 0;
}

int ioapic_route_irq(ioapic_t *ioapic, u32 route, u8 vec) {
  if (!ioapic)
    return -EINVAL;

  if (!atomic_load_explicit(&ioapic->initialized, memory_order_acquire)) {
    pr_error("[ioapic] routing requested before initialization");
    return -ENOENT;
  }

  if (vec < 0x20 || vec == 0xFF) {
    pr_error("[ioapic] invalid interrupt vector: 0x%x", (unsigned)vec);
    return -EINVAL;
  }

  if ((uptr)route < ioapic->ioapic_gsi_base)
    return -EINVAL;

  uptr offset = (uptr)route - ioapic->ioapic_gsi_base;

  if (offset >= ioapic->redir_count) {
    pr_error("[ioapic] gsi %u is outside this controller's range", route);
    return -EINVAL;
  }

  u8 low_reg = (u8)(IOAPIC_REDIR_BASE + 2U * (u32)offset);
  u8 high_reg = low_reg + 1;

  u32 low = (u32)vec | IOAPIC_REDIR_MASK;
  u32 high = (u32)ioapic->dest_apic_id << 24;

  spin_lock(&ioapic->lock);

  ioapic_write_reg_locked(ioapic, low_reg, low);
  ioapic_write_reg_locked(ioapic, high_reg, high);

  ioapic_write_reg_locked(ioapic, low_reg, (u32)vec);

  spin_unlock(&ioapic->lock);

  pr_info("[ioapic] routed gsi %u to vector 0x%x (input %u, apic id %u)", route,
          (unsigned)vec, (u32)offset, (unsigned)ioapic->dest_apic_id);

  return 0;
}

int ioapic_route_gsi(u32 gsi, u8 vec) {
  acpi_parse_res_t *res = acpi_get_parse_res();

  if (!res || !res->ioapics.data || !res->ioapics.count)
    return -ENOENT;

  ioapic_t *ioapics = (ioapic_t *)res->ioapics.data;

  for (size_t i = 0; i < res->ioapics.count; ++i) {
    ioapic_t *ioapic = &ioapics[i];

    if (!atomic_load_explicit(&ioapic->initialized, memory_order_acquire)) {
      continue;
    }

    if ((uptr)gsi < ioapic->ioapic_gsi_base)
      continue;

    uptr offset = (uptr)gsi - ioapic->ioapic_gsi_base;

    if (offset >= ioapic->redir_count)
      continue;

    return ioapic_route_irq(ioapic, gsi, vec);
  }

  pr_error("[ioapic] no initialized controller owns gsi %u", gsi);
  return -ENOENT;
}

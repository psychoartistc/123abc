#include <hardware/acpi/parse.h>
#include <hardware/lapic.h>
#include <mm/virtual/page_table.h>
#include <stddef.h>
#include <utils/int.h>
#include <utils/panic.h>

#define LAPIC_EOI 0x0B0

static volatile u32 *s_lapicMmio = NULL;

void lapic_init() {
  acpi_parse_res_t *res = acpi_get_parse_res();
  if (!res->lapic_phys)
    panic("[lapic] no lapic found, cannot proceed without a lapic");

  uintptr_t virt = (uintptr_t)p2v(res->lapic_phys);

  page_table_t *pt = kernel_pt();
  int r = pt_map(pt, virt, res->lapic_phys, P_READ | P_WRITE | P_NOCACHE);
  if (r < 0)
    panic("[lapic] failed to map lapic mmio: %d, proceed without a lapic", r);

  s_lapicMmio = (u32 *)virt;
}

void lapic_eoi(void) {
  assert(s_lapicMmio);

  s_lapicMmio[LAPIC_EOI / sizeof(u32)] = 0;
}

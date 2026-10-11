#include <arch/clock/clocksource.h>
#include <arch/control.h>
#include <arch/irq/exceptions.h>
#include <arch/paging.h>
#include <arch/stack/alloc.h>
#include <arch/stack/enter.h>
#include <arch/stack/utils.h>
#include <arch/tables/gdt.h>
#include <arch/tables/ist.h>
#include <drivers/api.h>
#include <hardware/acpi/parse.h>
#include <limine.h>
#include <limine_requests.h>
#include <mm/kern/slub.h>
#include <mm/physical/allocator.h>
#include <mm/virtual/page_table.h>
#include <mm/virtual/vma/vma.h>
#include <scheduler/smp/cpus.h>
#include <security/crypto/rng.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <utils/logging.h>
#include <utils/memory.h>
#include <utils/panic.h>
#include <utils/symbols.h>

void bsp_entry(void *arg) {
  (void)arg;
  pr_info("hello from bsp stack! at 0x%p", read_rsp());

  irq_enable();
  dump_cpu_features();
  acpi_tables_parse();
  drivers_initialize_some(DRV_INIT_ORDER_CLOCK_SOURCE);

  clocksource_t *source = csources_pick_best();
  if (!source)
    pr_error("no clock source found");

  int r = source->on_start(1000000, source->arg);
  if (r < 0)
    pr_error("%d", r);

  hcf();
}

void kmain(void) {
  symbols_init();

  drivers_initialize_some(DRV_INIT_ORDER_URGENT);

  rng_init();
  cpus_init();
  pmem_init();

  page_table_t *pt = kernel_pt_init();
  pt_switch(pt);
  vma_init();

  drivers_initialize_some(DRV_INIT_ORDER_ACPI_PARSER);

  kmem_init();
  kmem_dump_caches();

  drivers_initialize_some(DRV_INIT_ORDER_NORMAL);
  cpu_t *bsp = current_cpu();
  uintptr_t kstack = stack_alloc_kern();

  bsp->kern_stack_top = kstack + (KERN_STACK_PAGES * PAGE_SIZE);
  for (size_t i = 0; i < IST_STACK_COUNT; ++i) {
    uintptr_t istack = stack_alloc_ist();

    bsp->ist_stack_tops[i] = istack + (IST_STACK_PAGES * PAGE_SIZE);
  }

  cpu_init(bsp);

  idt_init();

  irq_dispatch_install();
  irq_exceptions_install();

  idt_load();

  stack_enter(bsp->kern_stack_top, bsp_entry, NULL);
  hcf();
}

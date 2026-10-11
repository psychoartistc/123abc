#include <arch/control.h>
#include <arch/cpu.h>
#include <arch/paging.h>
#include <utils/branching.h>
#include <utils/logging.h>

#define MSR_GS_BASE 0xC0000101

void set_current_cpu(cpu_t *c) { wrmsr(MSR_GS_BASE, (u64)c); }

struct cpu *current_cpu() { return (struct cpu *)rdmsr(MSR_GS_BASE); }

noreturn void ap_entry(struct limine_mp_info *info) {
  (void)info;

  hcf();
  unreachable();
}

void cpu_init(cpu_t *cpu) {
  if (unlikely(!cpu))
    return;

  tss_init(&cpu->tss);
  tss_rzero_stack_set(&cpu->tss, cpu->kern_stack_top);

  for (size_t i = 0; i < IST_STACK_COUNT; ++i)
    ist_set_stack_top(&cpu->ist, i, cpu->ist_stack_tops[i]);

  ist_install(&cpu->ist, &cpu->tss);

  gdt_init(&cpu->gdt, (u64)&cpu->tss.e, sizeof(tss_entry_t) - 1);
  gdt_load(&cpu->gdt);
  /*
  for (size_t i = 0; i < GDT_ENTRIES; i++) {
    gdt_entry_t en = cpu->gdt.entries[i];
    printk("%d) ", i);

    u8 *ptr = (u8 *)&en;
    for (size_t i = 0; i < sizeof(gdt_entry_t); i++) {
      u8 c = ptr[i];
      printk("0x%llx ", c);
    }
    printk("\n");
  } */

  tss_load(&cpu->tss, GDT_TSS_SELECTOR);
}

void dump_cpu_features() {
  bool tscdeadline = lapic_tsc_deadline_supported();
  bool gbpages = megapages_supported();
  bool invtsc = invariant_tsc_supported();

  pr_info("cpu features:");
  pr_info("\tlapic tsc deadline mode supported: %s",
          tscdeadline ? "yes" : "no");
  pr_info("\t1gb pages supported: %s", gbpages ? "yes" : "no");
  pr_info("\tinvariant tsc clock supported: %s", invtsc ? "yes" : "no");
}

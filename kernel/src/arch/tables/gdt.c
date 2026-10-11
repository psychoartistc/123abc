#include <arch/cpu.h>
#include <arch/tables/gdt.h>
#include <utils/branching.h>
#include <utils/logging.h>

void gdt_init(gdt_t *g, u64 tss_base, u32 tss_lim) {
  if (unlikely(!g))
    return;

  g->gdt_ptr.limit = sizeof(g->entries) - 1;
  g->gdt_ptr.base = (u64)g->entries;

  gdt_set_gate(g, 0, 0, 0, 0, 0);

  gdt_set_gate(g, 1, 0, 0, 0x9A, 0x20);
  gdt_set_gate(g, 2, 0, 0, 0x92, 0x00);

  gdt_set_tss_gate(g, GDT_TSS_INDEX, tss_base, tss_lim);
}

void gdt_load(gdt_t *g) {
  if (unlikely(!g))
    return;

  asm volatile("lgdt %0\n\t"

               "mov $0x10, %%ax\n\t"
               "mov %%ax, %%ds\n\t"
               "mov %%ax, %%ss\n\t"
               "mov %%ax, %%es\n\t"
               "mov %%ax, %%fs\n\t"

               "pushq $0x08\n\t"
               "leaq 1f(%%rip), %%rax\n\t"
               "pushq %%rax\n\t"
               "lretq\n\t"
               "1:\n\t"
               :
               : "m"(g->gdt_ptr)
               : "rax", "memory");

  pr_info("[gdt] initialized on core %zu", current_cpu()->idx);
}

void gdt_set_tss_gate(gdt_t *g, size_t num, u64 base, u32 lim) {
  if (unlikely(!g) || num + 1 >= GDT_ENTRIES)
    return;

  gdt_set_gate(g, num, (u32)base, lim, 0x89, 0x00);

  u8 *upper = (u8 *)&g->entries[num + 1];
  upper[0] = (u8)(base >> 32);
  upper[1] = (u8)(base >> 40);
  upper[2] = (u8)(base >> 48);
  upper[3] = (u8)(base >> 56);
  upper[4] = 0;
  upper[5] = 0;
  upper[6] = 0;
  upper[7] = 0;
}

void gdt_set_gate(gdt_t *g, size_t num, u32 base, u32 limit, u8 access,
                  u8 gran) {
  if (unlikely(!g))
    return;

  g->entries[num].base_low = (base & 0xFFFF);
  g->entries[num].base_middle = ((base >> 16) & 0xFF);
  g->entries[num].base_high = ((base >> 24) & 0xFF);

  g->entries[num].limit_low = (limit & 0xFFFF);

  g->entries[num].granularity = (((limit >> 16) & 0x0F) | (gran & 0xF0));

  g->entries[num].access = access;
}

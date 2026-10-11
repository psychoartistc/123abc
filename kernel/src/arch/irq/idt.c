#include <arch/irq/idt.h>
#include <utils/branching.h>
#include <utils/errno.h>
#include <utils/logging.h>
#include <utils/memory.h>

static idt_entry_t s_entries[IDT_ENTRY_COUNT] = {0};
static idt_ptr_t s_idtPtr = {0};

void idt_init() {
  memset(s_entries, 0, sizeof(idt_entry_t) * IDT_ENTRY_COUNT);
  s_idtPtr.limit = sizeof(s_entries) - 1;
  s_idtPtr.base = (u64)s_entries;
}

void idt_clear_gate(u8 vec) {
  idt_entry_t *entry = &s_entries[vec];
  memset(entry, 0, sizeof(idt_entry_t));
}

int idt_set_gate(u8 vec, idt_handler_t handler, u16 selector, u8 ist,
                 u8 type_attr) {
  if (unlikely(!handler || ist > 7))
    return -EINVAL;

  u64 address = (u64)handler;

  idt_entry_t *gate = &s_entries[vec];

  gate->offset_low = (u16)(address & 0xFFFF);

  gate->selector = selector;

  gate->ist = (u8)(ist & 0x07);
  gate->type_attr = type_attr;

  gate->offset_middle = (u16)((address >> 16) & 0xFFFF);

  gate->offset_high = (u32)((address >> 32) & 0xFFFFFFFF);

  gate->reserved = 0;

  return 0;
}

void idt_load() {
  asm volatile("lidt %0" : : "m"(s_idtPtr) : "memory");
  pr_info("[idt] loaded idt at 0x%p", s_idtPtr.base);
}

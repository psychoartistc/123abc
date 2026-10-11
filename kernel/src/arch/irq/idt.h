#ifndef IDT_H_
#define IDT_H_

#include <utils/attributes.h>
#include <utils/int.h>

/*
 * irq/idt.h
 * x86 interrupt descriptor table */

#define IDT_ENTRY_COUNT 256

typedef struct interrupt_frame {
  uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
  uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax, int_no, err_code;
  uint64_t rip, cs, rflags, rsp, ss;
} __PACKED interrupt_frame_t;

typedef struct idt_entry {
  u16 offset_low;
  u16 selector;

  u8 ist;

  u8 type_attr;

  u16 offset_middle;
  u32 offset_high;

  u32 reserved;
} __PACKED idt_entry_t;

typedef struct idt_ptr {
  u16 limit;
  u64 base;
} __PACKED idt_ptr_t;

typedef void (*idt_handler_t)();

void idt_init();
void idt_clear_gate(u8 vec);
int idt_set_gate(u8 vec, idt_handler_t handler, u16 selector, u8 ist,
                 u8 type_attr);
void idt_load();

#endif

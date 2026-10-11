#include <arch/register_dump.h>
#include <utils/logging.h>

void register_dump() {
  printk("\nregister dump:");
  unsigned long rax, rbx, rcx, rdx;
  unsigned long rsi, rdi, rbp, rsp;
  unsigned long r8, r9, r10, r11;
  unsigned long r12, r13, r14, r15;
  unsigned long rip;

  asm volatile("mov %%rax, %0" : "=r"(rax));
  asm volatile("mov %%rbx, %0" : "=r"(rbx));
  asm volatile("mov %%rcx, %0" : "=r"(rcx));
  asm volatile("mov %%rdx, %0" : "=r"(rdx));
  asm volatile("mov %%rsi, %0" : "=r"(rsi));
  asm volatile("mov %%rdi, %0" : "=r"(rdi));
  asm volatile("mov %%rbp, %0" : "=r"(rbp));
  asm volatile("mov %%rsp, %0" : "=r"(rsp));
  asm volatile("mov %%r8,  %0" : "=r"(r8));
  asm volatile("mov %%r9,  %0" : "=r"(r9));
  asm volatile("mov %%r10, %0" : "=r"(r10));
  asm volatile("mov %%r11, %0" : "=r"(r11));
  asm volatile("mov %%r12, %0" : "=r"(r12));
  asm volatile("mov %%r13, %0" : "=r"(r13));
  asm volatile("mov %%r14, %0" : "=r"(r14));
  asm volatile("mov %%r15, %0" : "=r"(r15));

  asm volatile("lea (%%rip), %0" : "=r"(rip));

  printk("\n");
  printk("RAX: 0x%p\tRBX: 0x%p\n", (void *)rax, (void *)rbx);
  printk("RCX: 0x%p\tRDX: 0x%p\n", (void *)rcx, (void *)rdx);
  printk("RSI: 0x%p\tRDI: 0x%p\n", (void *)rsi, (void *)rdi);
  printk("RBP: 0x%p\tRSP: 0x%p\n", (void *)rbp, (void *)rsp);
  printk("R8:  0x%p\tR9:  0x%p\n", (void *)r8, (void *)r9);
  printk("R10: 0x%p\tR11: 0x%p\n", (void *)r10, (void *)r11);
  printk("R12: 0x%p\tR13: 0x%p\n", (void *)r12, (void *)r13);
  printk("R14: 0x%p\tR15: 0x%p\n", (void *)r14, (void *)r15);
  printk("RIP: 0x%p\n", (void *)rip);
}

void dump_frame(interrupt_frame_t *f) {
  printk("\n");
  printk("RAX: 0x%p\tRBX: 0x%p\n", (void *)f->rax, (void *)f->rbx);
  printk("RCX: 0x%p\tRDX: 0x%p\n", (void *)f->rcx, (void *)f->rdx);
  printk("RSI: 0x%p\tRDI: 0x%p\n", (void *)f->rsi, (void *)f->rdi);
  printk("RBP: 0x%p\tRSP: 0x%p\n", (void *)f->rbp, (void *)f->rsp);
  printk("R8:  0x%p\tR9:  0x%p\n", (void *)f->r8, (void *)f->r9);
  printk("R10: 0x%p\tR11: 0x%p\n", (void *)f->r10, (void *)f->r11);
  printk("R12: 0x%p\tR13: 0x%p\n", (void *)f->r12, (void *)f->r13);
  printk("R14: 0x%p\tR15: 0x%p\n", (void *)f->r14, (void *)f->r15);
  printk("RIP: 0x%p\n", (void *)f->rip);
}

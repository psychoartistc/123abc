#include <arch/control.h>

void irq_wait() { asm volatile("hlt"); }
void irq_enable() { asm volatile("sti"); }
void irq_disable() { asm volatile("cli"); }
void pause() { asm volatile("pause"); }

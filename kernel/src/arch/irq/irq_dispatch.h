#ifndef IRQ_DISPATCH_H_
#define IRQ_DISPATCH_H_

/*
 * irq_dispatch.h
 * interrupt dispatcher in one isr */

#include <arch/irq/idt.h>

typedef void (*irq_handler_t)(interrupt_frame_t *frame, void *arg);

void irq_dispatch_install();
void irq_register(u8 vec, irq_handler_t handler, void *arg);
void irq_unregister(u8 at);

void irq_dispatch(interrupt_frame_t *frame);

#endif

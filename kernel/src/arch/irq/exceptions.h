#ifndef IRQ_EXCEPTIONS_H_
#define IRQ_EXCEPTIONS_H_

/*
 * irq/exceptions.h
 * cpu exceptions and their handlers */

#define CPU_EXCEPT_PAGE_FAULT 14
#define CPU_EXCEPT_DOUBLE_FAULT 8
#define CPU_EXCEPT_NMI 2
#define CPU_EXCEPT_DEBUG 1
#define CPU_EXCEPT_MACHINE_CHECK 18

#include <arch/irq/irq_dispatch.h>

void irq_exceptions_install();

#endif

#ifndef PIC8259_H_
#define PIC8259_H_

/*
 * pic8259.h
 * one function to... disable the 8259 pic
 * this is done for compatibility with ioapics */

#include <arch/io/ports.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA5

void pic8259_disable();

#endif

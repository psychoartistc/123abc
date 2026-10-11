#ifndef REGISTER_DUMP_H_
#define REGISTER_DUMP_H_

/*
 * register_dump.h
 * register dump function for panic()*/

#include <arch/irq/idt.h>

void register_dump();
void dump_frame(interrupt_frame_t *);

#endif

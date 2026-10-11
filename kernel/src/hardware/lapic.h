#ifndef LAPIC_H_
#define LAPIC_H_

/*
 * hardware/lapic.h
 * local advanced programmable interrupt controller */

#define LAPIC_EOI 0x0B0

void lapic_init();
void lapic_eoi(void);

#endif

#ifndef CONTROL_H_
#define CONTROL_H_

#include <stdnoreturn.h>

void irq_wait();
void irq_enable();
void irq_disable();
void pause();

static inline noreturn void hcf() {
  for (;;)
    irq_wait();
}

#endif // !CONTROL_H_

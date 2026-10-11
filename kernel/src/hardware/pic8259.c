#include <arch/io/ports.h>
#include <hardware/pic8259.h>

static inline void io_wait(void) { outb(0x80, 0); }

void pic8259_disable() {
  // master
  outb(PIC1_DATA, 0xFF);
  io_wait();

  // slave
  outb(PIC2_DATA, 0xFF);
  io_wait();
}

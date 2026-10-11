#include <arch/control.h>
#include <arch/irq/exceptions.h>
#include <arch/paging.h>
#include <arch/register_dump.h>
#include <stddef.h>
#include <utils/branching.h>
#include <utils/logging.h>
#include <utils/panic.h>

static const char *EXCEPTION_NAMES[32] = {"Divide error",
                                          "Debug",
                                          "NMI interrupt",
                                          "Breakpoint",
                                          "Overflow",
                                          "BOUND range exceeded",
                                          "Invalid opcode",
                                          "Device not available",
                                          "Double fault",
                                          "Coprocessor segment overrun",
                                          "Invalid TSS",
                                          "Segment not present",
                                          "Stack segment fault",
                                          "General protection",
                                          "Page fault",
                                          "Reserved",
                                          "x87 FPU floating point rror",
                                          "Alignment check",
                                          "Machine check",
                                          "SIMD floating point exception",
                                          "Virtualization exception",
                                          "Control protection exception",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Hypervisor injection exception",
                                          "VMM communication exception",
                                          "Security exception",
                                          "Reserved"};

static void irq_except_handler(interrupt_frame_t *frame, void *arg) {
  (void)arg;

  irq_disable();
  u64 intNo = frame->int_no;
  if (unlikely(intNo > 32))
    return;

  if (intNo == CPU_EXCEPT_PAGE_FAULT) {
    u64 cr2 = read_cr2();
    panic("page fault at 0x%p", cr2);

    unreachable();
    hcf();
  }

  const char *exName = EXCEPTION_NAMES[intNo];

  panic("cpu exception caught: %s (int no. %zu)", exName, intNo);

  unreachable();
  hcf();
}

void irq_exceptions_install() {
  for (u8 i = 0; i < 32; i++)
    irq_register(i, irq_except_handler, NULL);
}

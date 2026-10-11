#include <arch/irq/exceptions.h>
#include <arch/irq/irq_dispatch.h>
#include <hardware/lapic.h>
#include <security/crypto/rng.h>
#include <stddef.h>
#include <utils/logging.h>

extern u64 g_isrStubTable[IDT_ENTRY_COUNT];

static idt_handler_t s_entries[IDT_ENTRY_COUNT] = {0};
static void *s_args[IDT_ENTRY_COUNT] = {0};

void irq_dispatch_install() {
  for (size_t i = 0; i < IDT_ENTRY_COUNT; i++) {

    u8 ist = 0;
    if (i == CPU_EXCEPT_DOUBLE_FAULT || i == CPU_EXCEPT_NMI ||
        i == CPU_EXCEPT_DEBUG || i == CPU_EXCEPT_MACHINE_CHECK) {
      // evil math
      u8 byte;

      do {
        rng_fill_bytes((char *)&byte, 1, 1);
      } while (byte >= 252);

      ist = (u8)(byte % 7u + 1u);
      pr_info("[irq_dispatch] using ist %d for interrupt vector %d", ist, i);
    }

    int r = idt_set_gate((u8)i, (idt_handler_t)g_isrStubTable[i], 0x08, (u8)ist,
                         0x8E);
    if (r < 0) {
      pr_error("[irq_dispatch] error %d when installing isrs", r);
      continue;
    }
  }
}

void irq_register(u8 vec, irq_handler_t handler, void *arg) {
  s_entries[vec] = handler;
  s_args[vec] = arg;
}

void irq_unregister(u8 at) {
  s_entries[at] = NULL;
  s_args[at] = NULL;
}

void irq_dispatch(interrupt_frame_t *frame) {

  u64 num = frame->int_no;
  if (num >= IDT_ENTRY_COUNT)
    return;

  irq_handler_t handler = s_entries[num];
  void *arg = s_args[num];

  if (handler)
    handler(frame, arg);
  lapic_eoi();
}

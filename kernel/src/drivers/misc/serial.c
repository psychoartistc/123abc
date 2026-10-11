#include <arch/io/ports.h>
#include <drivers/api.h>
#include <stddef.h>
#include <utils/logging.h>
#include <utils/printf.h>

#include <utils/errno.h>

#define COM1 0x3F8

#define COM1_DATA (COM1 + 0)
#define COM1_INTERRUPT (COM1 + 1)
#define COM1_BAUD_LOW (COM1 + 0)
#define COM1_BAUD_HIGH (COM1 + 1)
#define COM1_FIFO_CTRL (COM1 + 2)
#define COM1_LINE_CTRL (COM1 + 3)
#define COM1_MODEM_CTRL (COM1 + 4)
#define COM1_LINE_STATUS (COM1 + 5)

static bool com1_transmit_empty() { return inb(COM1_LINE_STATUS) & 0x20; }
static void com1_put(char c) {
  while (!com1_transmit_empty())
    ;
  outb(COM1_DATA, c);
}

static void com1_puts(const char *c) {
  for (size_t i = 0; c[i] != '\0'; i++) {
    if (c[i] == '\n') {
      com1_put('\r');
    }
    com1_put(c[i]);
  }
}

static int com1_init() {
  outb(COM1_INTERRUPT, 0x00);
  outb(COM1_LINE_CTRL, 0x80);
  outb(COM1_BAUD_LOW, 0x03);
  outb(COM1_BAUD_HIGH, 0x00);
  outb(COM1_LINE_CTRL, 0x03);
  outb(COM1_FIFO_CTRL, 0xC7);
  outb(COM1_MODEM_CTRL, 0x0B);

  outb(COM1_MODEM_CTRL, 0x1E);
  outb(COM1_DATA, 0xAE);
  if (inb(COM1_DATA) != 0xAE) {
    return -EINVAL;
  }

  outb(COM1_MODEM_CTRL, 0x0F);
  com1_puts("\n\n\n\n");

  return 0;
}

static void serial_printk(char c, void *arg) {
  (void)arg;
  com1_put(c);
}

static void com1_log_poller(u8 level, const char *fmt, va_list va) {
  if (level != L_PRINTK) {
    com1_puts("[ ");
    com1_puts(logs_level_str(level));
    com1_puts(" ] ");
  }

  vfctprintf(serial_printk, NULL, fmt, va);
  if (level != L_PRINTK)
    com1_put('\n');
}

static int com1_drv_init() {
  int r = com1_init();
  if (r < 0)
    return r;

  logs_add_handler(com1_log_poller);
  return 0;
}

REGISTER_DRIVER(com1_driver, "com1 serial",
                "handles logs and polls them to com1 serial", "1.0.0",
                DRVLICENSE_DEFAULT, DRV_INIT_ORDER_URGENT, com1_drv_init, NULL);

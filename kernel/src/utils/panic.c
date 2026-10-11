#include <arch/control.h>
#include <arch/register_dump.h>
#include <arch/stack/trace.h>
#include <scheduler/spinlock.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <utils/branching.h>
#include <utils/logging.h>
#include <utils/panic.h>
#include <utils/rdfile.h>

static atomic_flag s_lock;

noreturn void vpanic(const char *fmt, va_list args) {
  if (!atomic_flag_test_and_set_explicit(&s_lock, memory_order_acquire))
    goto skip_pause;

  // if the if statement passes that means the lock is locked
  // we have nothing other to do than spin around because the
  // other core has already printed everything
  hcf();
  unreachable();

skip_pause:

  irq_disable();

  printk("\n\n");
  pr_file("noooo.ascii");
  printk("\nkernel panic\n");
  printk("error: ");
  vprintk(fmt, args);
  printk("\n");
  register_dump();
  printk("\n");

  stack_trace();

  hcf();
  unreachable();

  // the spinlock never released to not let other
  // cores print the same stuff again
}

noreturn void panic(const char *fmt, ...) {
  irq_disable();
  va_list va;
  va_start(va, fmt);

  vpanic(fmt, va);
  va_end(va);
}

void panic_init() { atomic_flag_clear(&s_lock); }

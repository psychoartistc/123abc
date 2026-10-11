#include <arch/stack/trace.h>
#include <utils/branching.h>
#include <utils/logging.h>
#include <utils/symbols.h>

static void symbol_info(uintptr_t ret) {
  if (symbols_available()) {
    const char *name = symbols_resolve(ret);

    printk("   [<0x%p>] %s\n", ret, name);
  } else
    printk("   [<0x%p>]\n", ret);
}

void stack_trace() {
  printk("stack trace:\n");
  if (unlikely(!symbols_available()))
    printk("symbol info not available, printing raw return addresses\n");

  uintptr_t *rbp = NULL;
  asm volatile("mov %%rbp, %0" : "=r"(rbp));

  while (rbp != NULL) {
    uintptr_t ret = rbp[1];
    if (ret == 0)
      break;

    symbol_info(ret);
    rbp = (uintptr_t *)*rbp;
  }
}

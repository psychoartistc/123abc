#include <arch/stack/alloc.h>
#include <mm/kern/slub.h>
#include <utils/panic.h>

uintptr_t stack_alloc(size_t mem) {
  void *ptr = kmalloc(mem);
  assert(ptr && "failure to allocate a stack, fatal");

  return (uintptr_t)ptr;
}

uintptr_t stack_alloc_kern() {
  return stack_alloc(KERN_STACK_PAGES * PAGE_SIZE);
}
uintptr_t stack_alloc_ist() {
  return stack_alloc(KERN_STACK_PAGES * PAGE_SIZE);
}

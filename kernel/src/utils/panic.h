#ifndef PANIC_H_
#define PANIC_H_

/*
 * panic.h
 * panic and assert function definitions */

#include <stdarg.h>
#include <stdnoreturn.h>
#include <utils/branching.h>

noreturn void panic(const char *fmt, ...);
noreturn void vpanic(const char *fmt, va_list args);
void panic_init();

#define assert(condition)                                                      \
  do {                                                                         \
    if (unlikely(!(condition))) {                                              \
      panic("assertion failed: %s, file %s, line %d", #condition, __FILE__,    \
            __LINE__);                                                         \
    }                                                                          \
  } while (0)

#endif // !PANIC_H_

#include <stddef.h>
#include <utils/branching.h>
#include <utils/logging.h>

static log_handler_t s_handlers[MAX_LOG_HANDLERS] = {0};
static size_t s_handlersCount = 0;

void logs_add_handler(log_handler_t handler) {
  s_handlers[s_handlersCount++] = handler;
}

void logs_poll(uint8_t l, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  logs_vpoll(l, fmt, va);
  va_end(va);
}

void logs_vpoll(uint8_t l, const char *fmt, va_list va) {
  if (unlikely(l > L_MAXLEVEL || !fmt))
    return;

  for (size_t i = 0; i < s_handlersCount; i++) {
    log_handler_t handler = s_handlers[i];
    if (!handler)
      continue;

    va_list args;
    va_copy(args, va);
    handler(l, fmt, args);
    va_end(args);
  }
  va_end(va);
}

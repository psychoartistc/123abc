#ifndef LOGGING_H_
#define LOGGING_H_

/*
 * logging.h
 * logging functions*/

#include <stdarg.h>
#include <stdint.h>

#define MAX_LOG_HANDLERS 64

#define L_TRACE 0
#define L_NOTE 1
#define L_DEBUG 2
#define L_INFO 3
#define L_WARN 4
#define L_ERROR 5
#define L_CRITICAL 6
#define L_PRINTK 7

#define L_MAXLEVEL L_PRINTK

typedef void (*log_handler_t)(uint8_t level, const char *fmt, va_list args);

void logs_add_handler(log_handler_t handler);
void logs_poll(uint8_t l, const char *fmt, ...);
void logs_vpoll(uint8_t l, const char *fmt, va_list va);

#define pr_trace(fmt, ...)                                                     \
  logs_poll(L_TRACE, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define pr_note(fmt, ...)                                                      \
  logs_poll(L_NOTE, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define pr_debug(fmt, ...)                                                     \
  logs_poll(L_DEBUG, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define pr_info(fmt, ...)                                                      \
  logs_poll(L_INFO, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define pr_warn(fmt, ...)                                                      \
  logs_poll(L_WARN, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define pr_error(fmt, ...)                                                     \
  logs_poll(L_ERROR, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define pr_critical(fmt, ...)                                                  \
  logs_poll(L_CRITICAL, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

#define printk(fmt, ...) logs_poll(L_PRINTK, fmt, ##__VA_ARGS__)
#define vprintk(fmt, va) logs_vpoll(L_PRINTK, fmt, va);

static inline const char *logs_level_str(uint8_t level) {
  switch (level) {
  case L_TRACE:
    return "TRACE";
  case L_NOTE:
    return "NOTE";
  case L_DEBUG:
    return "DEBUG";
  case L_INFO:
    return "INFO";
  case L_WARN:
    return "WARN";
  case L_ERROR:
    return "ERROR";
  case L_CRITICAL:
    return "CRIT";
  case L_PRINTK:
    return "PRINTK";
  default:
    return "???";
  }
}

#endif // !LOGGING_H_

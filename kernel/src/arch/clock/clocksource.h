#ifndef CLOCK_SOURCE_H_
#define CLOCK_SOURCE_H_

/*
 * clocksource.h
 * clock source type and it's abstraction */

#define CLOCK_SOURCE_LAPICTSC 0
#define CLOCK_SOURCE_HPET 1

#include <stdbool.h>
#include <utils/int.h>

typedef bool (*csource_check_available_fn_t)(void *arg);
typedef int (*csource_start_fn_t)(u64 initial_deadline, void *arg);
typedef int (*csource_set_next_deadline_fn_t)(u64 d, void *arg);

typedef struct {
  char *name;
  void *arg;

  // checks for the clock source availability without launching it
  csource_check_available_fn_t available_check;
  // starts the clock source with the initial deadline
  // if is_local is true then this is called on every core
  csource_start_fn_t on_start;
  // changes the deadline, the clock should fire an interrupt
  // after `d` nanoseconds passed
  csource_set_next_deadline_fn_t on_deadline_set;

  u8 type;
  bool is_local;
} clocksource_t;

// this must be called by the clock source on tick
void csources_on_tick();
void csources_add_source(clocksource_t source);
clocksource_t *csources_pick_best();

#endif

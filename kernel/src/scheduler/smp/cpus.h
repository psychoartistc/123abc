#ifndef CPUS_H_
#define CPUS_H_

#include <arch/cpu.h>
#include <limine_requests.h>
#include <utils/int.h>
#include <utils/math.h>

/*
 * cpus.h
 * launch all cpus */

typedef struct cpus_state {
  struct cpu cpus[MAX_CORES];
  u32 cpus_online; // atomic
} cpus_state_t;

static inline u64 cpus_count() {
  if (!mp_request.response)
    return 1;

  return MIN(mp_request.response->cpu_count, MAX_CORES);
}

void cpus_init();

#endif

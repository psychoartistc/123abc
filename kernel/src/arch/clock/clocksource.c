#include <arch/clock/clocksource.h>
#include <stdatomic.h>
#include <stddef.h>
#include <utils/logging.h>

#define MAX_CLOCKSOURCES 32

clocksource_t *s_currentSource = NULL;

clocksource_t s_clocksources[32] = {0};
size_t s_clocksourcesCount = 0;

void csources_add_source(clocksource_t source) {
  s_clocksources[atomic_fetch_add(&s_clocksourcesCount, 1)] = source;
}

clocksource_t *csources_pick_best() {
  clocksource_t *candidate = NULL;
  bool cached = false;
  if (cached)
    return candidate;

  int minType = 1e9;
  for (u64 i = 0; i < s_clocksourcesCount; i++) {
    clocksource_t csource = s_clocksources[i];

    if (!csource.available_check)
      continue;

    bool res = csource.available_check(csource.arg);

    if (!res && csource.name) {
      pr_info("[clocksource] clock source \'%s\' failed the availability check",
              csource.name);
      continue;
    }

    if (csource.type < minType) {
      minType = csource.type;
      candidate = &s_clocksources[i];
    }
  }

  if (candidate && candidate->name)
    pr_info("[clocksource] picked clock source \'%s\'", candidate->name);

  cached = true;

  s_currentSource = candidate;
  return candidate;
}

static u64 s_ms = 0;
void csources_on_tick() {
  // int r = s_currentSource->on_deadline_set(1000000, s_currentSource->arg);
  // if (r < 0)
  // pr_error("%d", r);

  pr_info("t");
  s_ms++;
  if (s_ms % 1000 == 0) {
    pr_info("%zu seconds", s_ms / 1000);
  }
}

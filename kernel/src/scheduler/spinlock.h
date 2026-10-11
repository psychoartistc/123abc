#ifndef SPINLOCK_H_
#define SPINLOCK_H_

/*
 * spinlock.h
 * the spinlock synchronization primitive */

#include <arch/control.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <utils/branching.h>

typedef struct spinlock {
  atomic_flag flag;
} spinlock_t;

static inline void spinlock_init(spinlock_t *s) {
  if (unlikely(!s))
    return;

  atomic_flag_clear(&s->flag);
}

static inline void spin_lock(struct spinlock *s) {
  if (unlikely(!s))
    return;

  if (!atomic_flag_test_and_set_explicit(&s->flag, memory_order_acquire))
    return;

  while (1) {
    pause();

    if (!atomic_flag_test_and_set_explicit(&s->flag, memory_order_acquire)) {
      break;
    }
  }
}

static inline void spin_unlock(struct spinlock *s) {
  if (unlikely(!s))
    return;

  atomic_flag_clear_explicit(&s->flag, memory_order_release);
}

#endif

#ifndef UTILS_MATH_H_
#define UTILS_MATH_H_

/*
 * math.h
 * mathemathic functions and operations */

#include <stddef.h>
#include <stdint.h>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static inline uint64_t div_ceil(uint64_t a, uint64_t b) {
  return (a + b - 1) / b;
}
static inline uint64_t div_floor(uint64_t a, uint64_t b) {
  return a / b; /* duh */
}

static inline size_t align_up(size_t n, size_t a) {
  return (n + (a - 1)) & ~(a - 1);
}

static inline size_t align_down(size_t n, size_t a) { return n & ~(a - 1); }

#endif

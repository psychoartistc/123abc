#ifndef ERR_H_
#define ERR_H_

/*
 * err.h
 * linux error handling
 * */

#include <stdbool.h>
#include <utils/branching.h>

#define MAX_ERRNO 4095
#define IS_ERR_VALUE(x)                                                        \
  unlikely((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)

static inline void *ERR_PTR(long error) { return (void *)error; }

#define INIT_ERR_PTR(error) ((void *)(error))

static inline long PTR_ERR(const void *ptr) { return (long)ptr; }

#define PTR_ERR_PCPU(ptr) (PTR_ERR((const void *)(const unsigned long)(ptr)))

static inline bool IS_ERR(const void *ptr) {
  return IS_ERR_VALUE((unsigned long)ptr);
}

#define IS_ERR_PCPU(ptr) (IS_ERR((const void *)(const unsigned long)(ptr)))

static inline bool IS_ERR_OR_NULL(const void *ptr) {
  return unlikely(!ptr) || IS_ERR_VALUE((unsigned long)ptr);
}

static inline void *ERR_CAST(const void *ptr) { return (void *)ptr; }

static inline int PTR_ERR_OR_ZERO(const void *ptr) {
  if (IS_ERR(ptr))
    return PTR_ERR(ptr);
  else
    return 0;
}
#endif // ERR_H_

#ifndef STACK_UTILS_H_
#define STACK_UTILS_H_

/*
 * stack/utils.h
 * utilities for using stacks */

#include <utils/int.h>

static inline uintptr_t read_rsp() {
  uintptr_t rsp;
  asm volatile("mov %%rsp, %0" : "=r"(rsp));
  return rsp;
}

#endif

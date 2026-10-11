#ifndef MSR_H_
#define MSR_H_

/*
 * msr.h
 * writing / reading to msr
 * */

#include <stddef.h>
#include <stdint.h>
#include <utils/int.h>

static inline u64 rdmsr(u32 msr) {
  uint32_t lo, hi;
  asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
  return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(u32 index, u64 value) {
  u32 eax = (u32)(value & 0xFFFFFFFF);
  u32 edx = (u32)(value >> 32);
  asm volatile("wrmsr" : : "c"(index), "a"(eax), "d"(edx));
}

#endif

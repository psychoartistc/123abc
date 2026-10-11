#ifndef BRANCHING_H_
#define BRANCHING_H_

/*
 * branching.h
 * branching optimizations
 * */

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define unreachable() (__builtin_unreachable())

#endif // !BRANCHING_H_

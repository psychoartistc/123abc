#ifndef STACK_ALLOC_H_
#define STACK_ALLOC_H_

/*
 * stack/alloc.h
 * function to allocate a stack*/

#include <stddef.h>
#include <utils/int.h>

#define KERN_STACK_PAGES 6
#define IST_STACK_PAGES 4

uintptr_t stack_alloc(size_t mem);
uintptr_t stack_alloc_kern();
uintptr_t stack_alloc_ist();

#endif

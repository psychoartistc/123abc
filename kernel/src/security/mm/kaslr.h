#ifndef KASLR_H_
#define KASLR_H_

/*
 * kaslr.h
 * kernel address space layout randomization */

#include <stddef.h>
#include <stdint.h>

#define VMALLOC_DEFAULT_WND_BASE 0xffffc90000000000

uintptr_t vmalloc_window_base();

#endif

#ifndef STACK_ENTER_H_
#define STACK_ENTER_H_

/*
 * stack/enter.h
 * function to enter a stack
 * defined in stack/enter_stub.S */

#include <utils/int.h>

void stack_enter(u64 top, void (*entry)(void *), void *arg);

#endif

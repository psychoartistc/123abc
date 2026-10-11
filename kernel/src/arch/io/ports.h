#ifndef PORTS_H_
#define PORTS_H_

/*
 * ports.h
 * x86 port io */

#include <stdint.h>
#include <utils/int.h>

static inline u8 inb(u16 port) {
  u8 ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outb(u16 port, u8 val) {
  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u16 inw(uint16_t port) {
  u16 ret;
  asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outw(u16 port, uint16_t val) {
  asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port) {
  u32 ret;
  asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outl(u16 port, u32 val) {
  asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

#endif // !PORTS_H_

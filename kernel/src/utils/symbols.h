#ifndef SYMBOLS_H_
#define SYMBOLS_H_

#include <fs/elf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * symbols.h
 * kernel file symbols */

typedef struct ksymbols {
  const char *strtab;
  size_t num_symbols;
  struct elf64_sym *symtab;
  bool available;
} ksymbols_t;

int symbols_init();
bool symbols_available();
const char *symbols_resolve(uintptr_t addr);

#endif

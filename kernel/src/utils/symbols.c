#include <fs/elf.h>
#include <limine_requests.h>
#include <utils/branching.h>
#include <utils/errno.h>
#include <utils/memory.h>
#include <utils/symbols.h>

static ksymbols_t s_ksymbols = {0};

int symbols_init() {
  if (unlikely(!kernel_file_request.response))
    return -ENOENT;

  struct limine_file *kernel_file =
      kernel_file_request.response->executable_file;

  struct elf64_ehdr *ehdr = (struct elf64_ehdr *)kernel_file->address;
  struct elf64_shdr *shdr =
      (struct elf64_shdr *)((uintptr_t)ehdr + ehdr->e_shoff);

  elf64_shdr_t *shstr_sec = &shdr[ehdr->e_shstrndx];
  char *shstrtab = (char *)((uintptr_t)ehdr + shstr_sec->sh_offset);

  for (size_t i = 0; i < ehdr->e_shnum; i++) {
    char *name = shstrtab + shdr[i].sh_name;

    if (strlcmp(name, 128, ".symtab", 8) == 0) {
      s_ksymbols.symtab = (elf64_sym_t *)((uintptr_t)ehdr + shdr[i].sh_offset);
      s_ksymbols.num_symbols = shdr[i].sh_size / sizeof(elf64_sym_t);
    } else if (strlcmp(name, 128, ".symtab", 8) == 0) {
      s_ksymbols.strtab = (char *)((uintptr_t)ehdr + shdr[i].sh_offset);
    }
  }

  s_ksymbols.available = true;

  return 0;
}

bool symbols_available() { return s_ksymbols.available; }

const char *symbols_resolve(uintptr_t addr) {
  if (unlikely(!s_ksymbols.available || !s_ksymbols.symtab ||
               !s_ksymbols.strtab))
    return "???";

  elf64_sym_t *symtab = s_ksymbols.symtab;
  for (size_t i = 0; i < s_ksymbols.num_symbols; i++) {
    if (addr >= symtab[i].st_value &&
        addr < (symtab[i].st_value + symtab[i].st_size))
      return s_ksymbols.strtab + symtab[i].st_name;
  }

  return "???";
}

#ifndef PAGING_H_
#define PAGING_H_

/*
 * paging.h
 * paging functions */

#include <arch/cpu.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <utils/int.h>

#define PAGES_IN_SUPERPAGE 512
#define PAGES_IN_MEGAPAGE 262144

#define PAGE_SIZE 4096
#define SUPERPAGE_SIZE (4096 * 512)
#define MEGAPAGE_SIZE (262144 * 4096)

#define MASK_4K 0x000FFFFFFFFFF000ULL
#define MASK_2M 0x000FFFFFFFE00000ULL
#define MASK_1G 0x000FFFFFC0000000ULL

#define P_X86_PRESENT (1ULL << 0)
#define P_X86_WRITABLE (1ULL << 1)
#define P_X86_USER (1ULL << 2)
#define P_X86_WRITETHRU (1ULL << 3)
#define P_X86_NOCACHE (1ULL << 4)
#define P_X86_HUGE (1ULL << 7)
#define P_X86_GLOBAL (1ULL << 8)
#define P_X86_NX (1ULL << 63)
#define P_X86_OWNED (1ULL << 9)

#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY (1ULL << 6)
#define PTE_PAT_HUGE (1ULL << 12)

#define PML4_INDEX(a) (((a) >> 39) & 0x1FF)
#define PDPT_INDEX(a) (((a) >> 30) & 0x1FF)
#define PD_INDEX(a) (((a) >> 21) & 0x1FF)
#define PT_INDEX(a) (((a) >> 12) & 0x1FF)

#define P_READ (1 << 0)
#define P_WRITE (1 << 1)
#define P_EXECUTE (1 << 2)
#define P_KERNEL (1 << 3)
#define P_NOCACHE (1 << 4)

#define TABLE_FLAGS_MASK                                                       \
  (P_X86_PRESENT | P_X86_WRITABLE | P_X86_USER | P_X86_WRITETHRU |             \
   P_X86_NOCACHE | P_X86_NX)

static inline void local_invlpg(uintptr_t v) {
  asm volatile("invlpg (%0)" ::"r"(v) : "memory");
}

static inline uint64_t read_cr3(void) {
  uint64_t v;
  asm volatile("mov %%cr3, %0" : "=r"(v));
  return v;
}

static inline void write_cr3(uint64_t cr3) {
  asm volatile("movq %0, %%cr3" : : "r"(cr3) : "memory");
}

static inline u64 read_cr2(void) {
  u64 v;
  asm volatile("mov %%cr2, %0" : "=r"(v));
  return v;
}

static inline bool megapages_supported() {
  static bool cache = false;
  static bool cached = false;
  if (!cached) {
    uint32_t eax, ebx, ecx, edx;

    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000001)
      return false;

    cpuid(0x80000001, &eax, &ebx, &ecx, &edx);

    cache = (edx & (1 << 26)) != 0;
    cached = true;
  }

  return cache;
}

static inline bool page_is_canonical(uintptr_t v) {
  int64_t s = (int64_t)v;
  return ((s << 16) >> 16) == s;
}

static inline uint64_t page_level_addr_mask(int level) {
  return level == 1 ? MASK_4K : level == 2 ? MASK_2M : MASK_1G;
}

static inline size_t pages_level(int level) {
  return (size_t)1 << (9 * (level - 1));
}

static inline size_t pages_chunk(uintptr_t virt, int level, size_t rem) {
  size_t span = pages_level(level);
  size_t off = (virt >> 12) & (span - 1);
  size_t n = span - off;
  return n < rem ? n : rem;
}

static inline uint64_t vmem_to_pte_protect(int vm) {
  uint64_t flags = 0;
  if (vm & P_WRITE)
    flags |= P_X86_WRITABLE;
  if (vm & P_NOCACHE)
    flags |= P_X86_NOCACHE;
  if (!(vm & P_EXECUTE))
    flags |= P_X86_NX;
  if (!(vm & P_KERNEL))
    flags |= P_X86_USER;
  return flags;
}

static inline int pte_to_vmem_protect(uint64_t pte) {
  return P_READ | (pte & P_X86_WRITABLE ? P_WRITE : 0) |
         (pte & P_X86_NOCACHE ? P_NOCACHE : 0) |
         (pte & P_X86_NX ? 0 : P_EXECUTE) | (pte & P_X86_USER ? 0 : P_KERNEL);
}

static inline uint64_t leaf_flags(int protect) {
  return vmem_to_pte_protect(protect) | P_X86_PRESENT |
         ((protect & P_KERNEL) ? P_X86_GLOBAL : 0);
}

#endif

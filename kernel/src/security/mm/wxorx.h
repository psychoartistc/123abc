#ifndef WXORX_H_
#define WXORX_H_

/*
 * wxorx.h
 * W^X security measure
 * simply explained, a page can only be executable or writable */

#include <arch/paging.h>
#include <stdbool.h>
#include <utils/errno.h>

static inline bool vmem_protect_wxorx(int protect) {
  return (protect & P_WRITE) && (protect & P_EXECUTE);
}

static inline bool pte_protect_wxorx(uint64_t e) {
  return (e & P_X86_WRITABLE) && !(e & P_X86_NX);
}

static inline int validate_vmem_protect(int protect) {
  if (vmem_protect_wxorx(protect))
    return -EPERM;

  return 0;
}

#endif

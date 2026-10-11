#ifndef MM_COMMON_H_
#define MM_COMMON_H_

/*
 * mm/common.h
 * common memory management functions */

#include <limine_requests.h>
#include <utils/int.h>

static inline void *p2v(uintptr_t phys) {
  return (void *)(phys + hhdm_request.response->offset);
}

static inline uintptr_t v2p(void *virt) {
  return (uintptr_t)(virt)-hhdm_request.response->offset;
}

#endif // !MM_COMMON_H_

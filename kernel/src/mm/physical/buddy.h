#ifndef BUDDY_H_
#define BUDDY_H_

#include <mm/physical/allocator.h>

enum {
  BUDDY_MAX_ORDER = 10,
  BUDDY_CACHE_ALIGN = 64,
  BUDDY_NIL_INDEX = UINT32_MAX,
};

typedef struct buddy_order_level {
  uint64_t head;

  uint32_t *next;
  uint64_t *free_bitmap;

  size_t block_count;

  uint8_t _cache_pad[BUDDY_CACHE_ALIGN - 32];
} buddy_order_level_t;

typedef struct buddy_allocator {
  _Alignas(BUDDY_CACHE_ALIGN) buddy_order_level_t levels[BUDDY_MAX_ORDER + 1];

  uint64_t frame_count;
  bool initialized;
} buddy_allocator_t;

static inline size_t div_ceil_size(size_t x, size_t y) {
  return x / y + (x % y != 0);
}

static inline size_t align_up_size(size_t x, size_t align) {
  return div_ceil_size(x, align) * align;
}

static inline uintptr_t align_up_ptr(uintptr_t x, uintptr_t align) {
  return (uintptr_t)div_ceil_size((size_t)x, (size_t)align) * align;
}

static inline uint64_t pack_head(uint32_t index, uint32_t tag) {
  return ((uint64_t)tag << 32) | index;
}

static inline uint32_t head_index(uint64_t head) {
  return (uint32_t)(head & UINT32_MAX);
}

static inline uint32_t head_tag(uint64_t head) {
  return (uint32_t)(head >> 32);
}

static size_t order_for_count(uint64_t count) {
  size_t order = 0;
  uint64_t size = 1;

  while (size < count && order <= BUDDY_MAX_ORDER) {
    size <<= 1;
    ++order;
  }

  return order;
}

#endif

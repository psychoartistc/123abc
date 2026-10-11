#ifndef SLUB_H_
#define SLUB_H_

/*
 * kmem.h
 * kernel slub allocator */

#include <arch/cpu.h>
#include <arch/paging.h>
#include <mm/common.h>
#include <scheduler/spinlock.h>
#include <stddef.h>
#include <utils/memory.h>

#define KMEM_MIN_CLASS 16u
#define KMEM_MAX_CLASS 4096u
#define KMEM_NUM_CLASSES 9
#define KMEM_SLAB_BYTES (PAGE_SIZE * 16)

#define KMEM_MAGIC_SLAB 0x534C4142u
#define KMEM_MAGIC_LARGE 0x4C415247u

#define KMEM_PTR_BITS 48
#define KMEM_TAG_BITS (64 - KMEM_PTR_BITS)
#define KMEM_PTR_MASK ((1ull << KMEM_PTR_BITS) - 1)
#define KMEM_SIGN_BIT (1ull << (KMEM_PTR_BITS - 1))

static const size_t kmem_class_sizes[KMEM_NUM_CLASSES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

typedef struct kmem_header {
  uint32_t magic;
  uint32_t class_idx;
  size_t alloc_size;
} kmem_header_t;

#define KMEM_HEADER_SIZE (sizeof(kmem_header_t))

typedef struct kmem_slab {
  uint32_t bump_index;

  uint32_t capacity;

  uint8_t *base;

  // can be used by:
  // cache->all_slabs (plain address in a push only list)
  // cache->spare_slabs (tagged ptr in a treiber stack)
  struct kmem_slab *next;
} kmem_slab_t;

typedef struct kmem_cache {
  const char *name;
  struct kmem_cache *next;

  uint64_t id;
  size_t obj_size;
  size_t slot_size;

  uint64_t free_stack;

  // push only
  kmem_slab_t *all_slabs;

  // treiber stack with a tagged ptr
  uint64_t spare_slabs;

  // semaphore
  // kmem_cache_destroy waits for this to be zero and only then frees slabs
  uint32_t active_ops;
  uint32_t destroying;

  kmem_slab_t *cpu_slab[MAX_CORES];
} kmem_cache_t;

void kmem_init();

void *kmalloc(size_t size);

void *kmem_alloc_large(size_t size);

void *kmalloc_from_cache(kmem_cache_t *cache);

int kmem_cache_init(kmem_cache_t *c, const char *name, size_t objsize);

void kfree_to_cache(kmem_cache_t *c, void *ptr);

int kmem_cache_destroy(kmem_cache_t *cache);

void kmem_dump_caches();

void kfree(void *ptr);
void *krealloc(void *ptr, size_t size);

static inline void *kzalloc(size_t size) {
  void *ptr = kmalloc(size);
  if (!ptr)
    return NULL;

  memset(ptr, 0, size);
  return ptr;
}

static inline void *kmalloc_objs(size_t count, size_t size) {
  if (count == 0 || size == 0)
    return NULL;

  if (size > SIZE_MAX / count)
    return NULL;

  size_t total = count * size;
  return kmalloc(total);
}

static inline void *kzalloc_objs(size_t count, size_t size) {
  void *ptr = kmalloc_objs(count, size);
  if (!ptr)
    return NULL;

  memset(ptr, 0, count * size);
  return ptr;
}

static inline void *kzalloc_from_cache(kmem_cache_t *cache) {
  void *a = kmalloc_from_cache(cache);
  if (!a)
    return NULL;

  memset(a, 0, cache->obj_size);

  return a;
}

#endif

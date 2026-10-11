#include <arch/control.h>
#include <mm/kern/slub.h>
#include <mm/virtual/vma/vma.h>

// void rng_fill_bytes(char *buff, size_t amount, size_t buffSize);
#include <security/crypto/rng.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <utils/dsa.h>
#include <utils/err.h>
#include <utils/errno.h>
#include <utils/logging.h>
#include <utils/math.h>
#include <utils/panic.h>

#include <scheduler/spinlock.h>

// spinlock for adding and deleting caches
// usually not used much
static spinlock_t s_cacheSpinlock;
static kmem_cache_t *s_cacheHead = NULL;
static kmem_cache_t s_kmallocCaches[KMEM_NUM_CLASSES];

static uint64_t s_maxId = 0;

static inline void kmem_freeslot_set_next(void *node, void *next) {
  *(void **)node = next;
}

static inline void *kmem_freeslot_get_next(void *node) {
  return *(void **)node;
}

// address packing stuff
// used to solve aba
static inline uint64_t kmem_pack(void *ptr, uint16_t tag) {
  uint64_t p = (uint64_t)(uintptr_t)ptr & KMEM_PTR_MASK;
  return p | ((uint64_t)tag << KMEM_PTR_BITS);
}

static inline void *kmem_unpack_ptr(uint64_t packed) {
  uint64_t p = packed & KMEM_PTR_MASK;
  if (p & KMEM_SIGN_BIT)
    p |= ~KMEM_PTR_MASK;
  return (void *)(uintptr_t)p;
}

static inline uint16_t kmem_unpack_tag(uint64_t packed) {
  return (uint16_t)(packed >> KMEM_PTR_BITS);
}

// treiber stack utils
static void kmem_freeobj_push(uint64_t *head, void *node) {
  uint64_t cur = atomic_load_explicit(head, memory_order_acquire);
  uint64_t next;
  do {
    kmem_freeslot_set_next(node, kmem_unpack_ptr(cur));
    next = kmem_pack(node, kmem_unpack_tag(cur) + 1);
  } while (!atomic_compare_exchange_weak_explicit(
      head, &cur, next, memory_order_acq_rel, memory_order_acquire));
}

static void *kmem_freeobj_pop(uint64_t *head) {
  uint64_t cur = atomic_load_explicit(head, memory_order_acquire);

  for (;;) {
    void *top = kmem_unpack_ptr(cur);
    if (top == NULL)
      return NULL;

    uint64_t next =
        kmem_pack(kmem_freeslot_get_next(top), kmem_unpack_tag(cur) + 1);

    if (atomic_compare_exchange_weak_explicit(
            head, &cur, next, memory_order_acq_rel, memory_order_acquire))
      return top;
  }
}

static void kmem_slab_list_push(kmem_slab_t **head, kmem_slab_t *slab) {
  kmem_slab_t *cur = atomic_load_explicit(head, memory_order_relaxed);
  do {
    slab->next = cur;
  } while (!atomic_compare_exchange_weak_explicit(
      head, &cur, slab, memory_order_release, memory_order_relaxed));
}

static void kmem_slab_push(uint64_t *head, kmem_slab_t *slab) {
  uint64_t cur = atomic_load_explicit(head, memory_order_acquire);
  uint64_t next;
  do {
    slab->next = kmem_unpack_ptr(cur);
    next = kmem_pack(slab, kmem_unpack_tag(cur) + 1);
  } while (!atomic_compare_exchange_weak_explicit(
      head, &cur, next, memory_order_acq_rel, memory_order_acquire));
}

static kmem_slab_t *kmem_slab_pop(uint64_t *head) {
  uint64_t cur = atomic_load_explicit(head, memory_order_acquire);

  for (;;) {
    kmem_slab_t *top = (kmem_slab_t *)kmem_unpack_ptr(cur);
    if (top == NULL)
      return NULL;

    uint64_t next = kmem_pack(top->next, kmem_unpack_tag(cur) + 1);

    if (atomic_compare_exchange_weak_explicit(
            head, &cur, next, memory_order_acq_rel, memory_order_acquire))
      return top;
  }
}

static int kmem_size_to_class(size_t size) {
  // binary search for the right class
  int left = 0;
  int right = KMEM_NUM_CLASSES - 1;

  while (left <= right) {
    int mid = left + (right - left) / 2;

    if (size <= kmem_class_sizes[mid]) {
      right = mid - 1;
    } else {
      left = mid + 1;
    }
  }

  // size doesn't fit into any slab, go for vmalloc
  return left < KMEM_NUM_CLASSES ? left : -1;
}

static inline uint32_t kmem_slab_capacity_for(size_t slot_size) {
  size_t header_area = align_up(sizeof(kmem_slab_t), 16);
  if (slot_size == 0 || slot_size > KMEM_SLAB_BYTES - header_area)
    return 0;

  return (uint32_t)((KMEM_SLAB_BYTES - header_area) / slot_size);
}

static kmem_slab_t *kmem_acquire_slab(kmem_cache_t *cache) {

  kmem_slab_t *spare = kmem_slab_pop(&cache->spare_slabs);
  if (spare != NULL)
    return spare;

  void *page = vmalloc(KMEM_SLAB_BYTES, P_READ | P_WRITE | P_KERNEL);
  if (!page)
    return ERR_PTR(-ENOMEM);

  kmem_slab_t *slab = (kmem_slab_t *)page;

  size_t header_area = align_up(sizeof(kmem_slab_t), 16);

  slab->base = (uint8_t *)page + header_area;
  slab->capacity = kmem_slab_capacity_for(cache->slot_size);

  atomic_store_explicit(&slab->bump_index, 0, memory_order_relaxed);

  assert(slab->capacity > 0);

  kmem_slab_list_push(&cache->all_slabs, slab);

  return slab;
}

static inline void *kmem_alloc_from_slab(kmem_cache_t *cache, int cpu) {
  for (;;) {
    kmem_slab_t *slab =
        atomic_load_explicit(&cache->cpu_slab[cpu], memory_order_acquire);
    if (slab != NULL) {
      // pre check on slabs that should be exhausted right now
      if (atomic_load_explicit(&slab->bump_index, memory_order_relaxed) <
          slab->capacity) {
        uint32_t idx = atomic_fetch_add_explicit(&slab->bump_index, 1u,
                                                 memory_order_relaxed);
        if (idx < slab->capacity)
          return slab->base + (size_t)idx * cache->slot_size;
      }
    }

    kmem_slab_t *fresh = kmem_acquire_slab(cache);
    if (IS_ERR(fresh))
      return fresh;

    kmem_slab_t *expected = slab;

    // try race
    if (atomic_compare_exchange_weak_explicit(&cache->cpu_slab[cpu], &expected,
                                              fresh, memory_order_acq_rel,
                                              memory_order_acquire))
      continue;

    // we lost, push spare slabs
    kmem_slab_push(&cache->spare_slabs, fresh);
  }
}

static void kmem_cache_release_slabs(kmem_cache_t *cache) {
  kmem_slab_t *slab =
      atomic_load_explicit(&cache->all_slabs, memory_order_acquire);
  while (!atomic_compare_exchange_weak_explicit(&cache->all_slabs, &slab, NULL,
                                                memory_order_acq_rel,
                                                memory_order_acquire))
    ; // retry with the value until we own the list

  while (slab != NULL) {
    kmem_slab_t *next = slab->next;

    vfree(slab);

    slab = next;
  }
}

static inline bool kmem_cache_enter(kmem_cache_t *c) {
  atomic_fetch_add_explicit(&c->active_ops, 1u, memory_order_acquire);

  if (unlikely(atomic_load_explicit(&c->destroying, memory_order_acquire))) {
    atomic_fetch_sub_explicit(&c->active_ops, 1u, memory_order_release);
    return false;
  }

  return true;
}

static inline void kmem_cache_exit(kmem_cache_t *c) {
  atomic_fetch_sub_explicit(&c->active_ops, 1u, memory_order_release);
}

void *kmem_alloc_large(size_t size) {
  if (size > SIZE_MAX - KMEM_HEADER_SIZE)
    return NULL;

  size_t total = size + KMEM_HEADER_SIZE;
  void *mem = vmalloc(total, P_READ | P_WRITE | P_KERNEL);
  if (!mem)
    return NULL;

  kmem_header_t *hdr = (kmem_header_t *)mem;
  hdr->magic = KMEM_MAGIC_LARGE;
  hdr->class_idx = 0;
  hdr->alloc_size = total;

  return (void *)((uint8_t *)mem + KMEM_HEADER_SIZE);
}

static void *kmem_cache_alloc_raw(kmem_cache_t *cache, uint32_t class_idx,
                                  size_t reqsize) {
  if (!kmem_cache_enter(cache))
    return NULL;

  void *slot = kmem_freeobj_pop(&cache->free_stack);

  if (slot == NULL) {
    int cpu = current_cpu()->idx;

    slot = kmem_alloc_from_slab(cache, cpu);
    if (IS_ERR(slot)) {
      kmem_cache_exit(cache);
      return NULL;
    }
  }

  kmem_header_t *hdr = (kmem_header_t *)slot;
  hdr->magic = KMEM_MAGIC_SLAB;
  hdr->class_idx = class_idx;
  hdr->alloc_size = reqsize;

  void *ptr = (uint8_t *)slot + KMEM_HEADER_SIZE;

  rng_fill_bytes((char *)ptr, reqsize, reqsize);

  kmem_cache_exit(cache);
  return ptr;
}

static void kmem_cache_free_raw(kmem_cache_t *cache, kmem_header_t *hdr) {
  if (!kmem_cache_enter(cache)) {

    pr_error("[kfree] free into cache '%s' racing with destroy, leaking",
             cache->name);
    return;
  }

  hdr->magic = 0;
  kmem_freeobj_push(&cache->free_stack, (void *)hdr);

  kmem_cache_exit(cache);
}

int kmem_cache_init(kmem_cache_t *c, const char *name, size_t objsize) {
  if (!c || objsize == 0)
    return -EINVAL;

  size_t slot_size = align_up(objsize + KMEM_HEADER_SIZE, 16);

  if (kmem_slab_capacity_for(slot_size) == 0)
    return -EINVAL;

  c->name = name;
  c->obj_size = objsize;
  c->slot_size = slot_size;
  c->id = atomic_fetch_add_explicit(&s_maxId, 1, memory_order_seq_cst);

  c->next = NULL;
  c->all_slabs = NULL;

  atomic_store_explicit(&c->free_stack, 0, memory_order_relaxed);
  atomic_store_explicit(&c->spare_slabs, 0, memory_order_relaxed);
  atomic_store_explicit(&c->active_ops, 0u, memory_order_relaxed);
  atomic_store_explicit(&c->destroying, 0u, memory_order_relaxed);

  for (int cpu = 0; cpu < MAX_CORES; cpu++)
    atomic_store_explicit(&c->cpu_slab[cpu], NULL, memory_order_relaxed);

  spin_lock(&s_cacheSpinlock);

  c->next = s_cacheHead;
  s_cacheHead = c;

  spin_unlock(&s_cacheSpinlock);

  return 0;
}

void kmem_dump_caches() {
  kmem_cache_t *current = NULL;
  spin_lock(&s_cacheSpinlock);

  LLIST_FOREACH(current, s_cacheHead) {
    pr_info("[kmem] cache ID: %llu, name: \'%s\', object size: %zu, "
            "slot size: %zu",
            (unsigned long long)current->id, current->name, current->obj_size,
            current->slot_size);
  }

  spin_unlock(&s_cacheSpinlock);
}

int kmem_cache_destroy(kmem_cache_t *cache) {
  if (cache == NULL)
    return -EINVAL;

  spin_lock(&s_cacheSpinlock);

  kmem_cache_t **link = &s_cacheHead;

  while (*link != NULL) {
    if (*link == cache) {
      *link = cache->next;
      cache->next = NULL;
      break;
    }

    link = &(*link)->next;
  }

  spin_unlock(&s_cacheSpinlock);

  atomic_store_explicit(&cache->destroying, 1u, memory_order_release);
  while (atomic_load_explicit(&cache->active_ops, memory_order_acquire) != 0)
    pause();

  kmem_cache_release_slabs(cache);

  return 0;
}

void kmem_init(void) {
  spinlock_init(&s_cacheSpinlock);
  for (int i = 0; i < KMEM_NUM_CLASSES; i++) {
    int r = kmem_cache_init(&s_kmallocCaches[i], "kmalloc_cache",
                            kmem_class_sizes[i]);
    if (r < 0)
      pr_error("[kmem] failed to initialize cache for class %d", i);
  }

  pr_info("[kmem] allocator initialized, %d classes, %d cores",
          KMEM_NUM_CLASSES, mp_request.response->cpu_count);
}

void kfree_to_cache(kmem_cache_t *c, void *ptr) {
  if (ptr == NULL)
    return;

  kmem_header_t *hdr = (kmem_header_t *)((uint8_t *)ptr - KMEM_HEADER_SIZE);
  pr_trace("[kfree] free 0x%p (size %zu, class idx %zu)", ptr, hdr->alloc_size,
           hdr->class_idx);

  if (hdr->magic == KMEM_MAGIC_LARGE) {
    // size_t total = hdr->alloc_size;
    pr_trace("[kfree] free 0x%p (size %zu, class idx %zu, largely allocated)",
             ptr, hdr->alloc_size, hdr->class_idx);

    vfree(hdr);

    return;
  }

  assert(hdr->magic == KMEM_MAGIC_SLAB && "double free");

  kmem_cache_free_raw(c, hdr);
}

void *kmalloc_from_cache(kmem_cache_t *cache) {
  if (unlikely(!cache))
    return NULL;

  if (unlikely(cache->obj_size == 0))
    return NULL;

  void *ptr = kmem_cache_alloc_raw(cache, 0, cache->obj_size);

  if (ptr)
    pr_trace("[kmalloc] allocate %zu bytes from cache %s", cache->obj_size,
             cache->name);

  return ptr;
}

void *kmalloc(size_t size) {
  if (size == 0)
    return NULL;

  int idx = kmem_size_to_class(size);
  if (idx < 0) {
    pr_trace("[kmalloc] slow path, size %zu too big", size);
    return kmem_alloc_large(size);
  }

  pr_trace("[kmalloc] fast path from bucket index %d", idx);

  kmem_cache_t *cache = &s_kmallocCaches[idx];

  void *ptr = kmem_cache_alloc_raw(cache, (uint32_t)idx, size);

  if (ptr)
    pr_trace("[kmalloc] allocate %zu bytes at 0x%p virt", size, ptr);

  return ptr;
}

void kfree(void *ptr) {
  if (ptr == NULL)
    return;

  kmem_header_t *hdr = (kmem_header_t *)((uint8_t *)ptr - KMEM_HEADER_SIZE);

  pr_trace("[kfree] free 0x%p (size %zu, class idx %zu)", ptr, hdr->alloc_size,
           hdr->class_idx);

  if (hdr->magic == KMEM_MAGIC_LARGE) {
    size_t size = hdr->alloc_size - KMEM_HEADER_SIZE;

    rng_fill_bytes((char *)ptr, size, size);

    vfree(hdr);
    return;
  }

  assert(hdr->magic == KMEM_MAGIC_SLAB && "double free");
  assert(hdr->class_idx < KMEM_NUM_CLASSES);

  kmem_cache_t *cache = &s_kmallocCaches[hdr->class_idx];

  kmem_cache_free_raw(cache, hdr);
}

void *krealloc(void *ptr, size_t size) {
  if (ptr == NULL)
    return kmalloc(size);

  if (size == 0) {
    kfree(ptr);
    return NULL;
  }

  kmem_header_t *hdr = (kmem_header_t *)((uint8_t *)ptr - KMEM_HEADER_SIZE);
  size_t old_usable;

  if (hdr->magic == KMEM_MAGIC_LARGE) {
    old_usable = hdr->alloc_size - KMEM_HEADER_SIZE;
  } else {
    assert(hdr->magic == KMEM_MAGIC_SLAB);
    assert(hdr->class_idx < KMEM_NUM_CLASSES);
    old_usable = s_kmallocCaches[hdr->class_idx].obj_size;
  }

  if (size <= old_usable)
    return ptr;

  void *newptr = kmalloc(size);
  if (newptr == NULL)
    return NULL;

  memcpy(newptr, ptr, old_usable);
  kfree(ptr);

  pr_trace("[krealloc] reallocate 0x%p into %zu byte region at 0x%p", ptr, size,
           newptr);
  return newptr;
}

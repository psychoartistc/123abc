#include <arch/paging.h>
#include <limine_requests.h>
#include <mm/virtual/vma/vma.h>
#include <security/mm/kaslr.h>
#include <utils/int.h>

static uint64_t kaslr_mix64(uint64_t x) {
  x += 0x9e3779b97f4a7c15ULL;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}

static uint64_t kaslr_entropy_seed(void) {
  uint64_t seed = 0;

  if (!entropy_request.response || entropy_request.response->value_count == 0)
    return 0;

  for (size_t i = 0; i < entropy_request.response->value_count; ++i) {
    seed ^= kaslr_mix64(entropy_request.response->values[i] +
                        i * 0x9e3779b97f4a7c15ULL);
    seed = kaslr_mix64(seed);
  }

  return seed;
}

static inline uintptr_t vmalloc_window_base_make() {
  uint64_t min_base = VMALLOC_DEFAULT_WND_BASE;

  uint64_t max_base = UINT64_MAX - (VMALLOC_WINDOW_SIZE - 1);

  if (min_base > max_base)
    return (uintptr_t)VMALLOC_DEFAULT_WND_BASE;

  uint64_t span = max_base - min_base;
  uint64_t slots = (span / PAGE_SIZE) + 1;

  uint64_t seed = kaslr_entropy_seed();

  if (seed == 0)
    return (uintptr_t)VMALLOC_DEFAULT_WND_BASE;

  uint64_t r = kaslr_mix64(seed);

  uint64_t threshold = (uint64_t)(-slots) % slots;

  while (r < threshold)
    r = kaslr_mix64(r);

  uint64_t offset = (r % slots) * PAGE_SIZE;

  return (uintptr_t)(min_base + offset);
}

uintptr_t vmalloc_window_base() {
  static bool initialized = false;
  static uptr wnd = 0x0;
  if (!initialized) {
    wnd = vmalloc_window_base_make();
    initialized = true;
  }

  return wnd;
}

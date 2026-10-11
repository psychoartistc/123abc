#include <mm/common.h>
#include <mm/physical/allocator.h>
#include <mm/virtual/page_table.h>
#include <mm/virtual/tlb.h>
#include <security/mm/wxorx.h>
#include <utils/errno.h>
#include <utils/panic.h>

static page_table_t s_kernelPt = {0};
static bool s_kPtReady = false;

static inline size_t page_idx(uintptr_t virt, int level) {
  return (virt >> (12 + 9 * (level - 1))) & 0x1FF;
}

static void zero_page(void *p) {
  volatile uint64_t *q = p;
  for (size_t i = 0; i < 512; i++)
    q[i] = 0;
}

static uintptr_t alloc_table(void) {
  pmalloc_res_t r = pmalloc(1);
  if (!r.ok)
    return 0;
  zero_page(p2v(r.addr));
  return r.addr;
}

static inline uint64_t *table_of(uint64_t entry) {
  return (uint64_t *)p2v(entry & MASK_4K);
}

static uint64_t *walk_create(page_table_t *pt, uintptr_t virt, int level,
                             bool user, int *err) {
  uint64_t *table = p2v(pt->pml4);

  for (int l = 4; l > level; l--) {
    uint64_t *e = &table[page_idx(virt, l)];
    uint64_t v = *e;

    if (!(v & P_X86_PRESENT)) {
      // make a table
      uintptr_t child = alloc_table();
      if (!child) {
        *err = -ENOMEM;
        return NULL;
      }

      v = child | P_X86_PRESENT | P_X86_WRITABLE | P_X86_OWNED |
          (user ? P_X86_USER : 0);

      atomic_store_explicit(e, v, memory_order_release);
    } else if (l < 4 && (v & P_X86_HUGE)) {

      *err = -EEXIST;
      return NULL;

    } else if (user && !(v & P_X86_USER)) {

      atomic_fetch_or_explicit(e, P_X86_USER, memory_order_release);
    }
    table = table_of(v);
  }
  return &table[page_idx(virt, level)];
}

static uint64_t *find_leaf(page_table_t *pt, uintptr_t virt, int *level_out,
                           bool want_user) {
  uint64_t *table = p2v(pt->pml4);

  for (int level = 4; level >= 1; level--) {
    uint64_t *e = &table[page_idx(virt, level)];
    uint64_t v = *e;

    if (!(v & P_X86_PRESENT))
      return NULL;

    if (level == 1 || (level < 4 && (v & P_X86_HUGE))) {
      *level_out = level;
      return e;
    }

    if (want_user && !(v & P_X86_USER))
      atomic_fetch_or_explicit(e, P_X86_USER, memory_order_release);

    table = table_of(v);
  }
  return NULL;
}

static int split_leaf(uint64_t *e, int level) {
  assert(level == 2 || level == 3);

  uint64_t old = *e;
  uintptr_t tphys = alloc_table();
  if (!tphys)
    return -ENOMEM;

  uint64_t *t = p2v(tphys);
  uint64_t mask = page_level_addr_mask(level);
  uint64_t base = old & mask;
  uint64_t step = level == 3 ? SUPERPAGE_SIZE : PAGE_SIZE;
  uint64_t flags = old & ~mask;

  if (level == 2) {
    flags &= ~(P_X86_HUGE | PTE_PAT_HUGE);
  }

  for (size_t i = 0; i < 512; i++)
    t[i] = (base + i * step) | flags;

  atomic_store_explicit(e,
                        tphys | P_X86_PRESENT | P_X86_WRITABLE | P_X86_OWNED |
                            (old & P_X86_USER),
                        memory_order_release);

  return 0;
}

static int clone_level(uint64_t *dst, const uint64_t *src, int level) {
  for (size_t i = (level == 4 ? 256 : 0); i < 512; i++) {
    uint64_t e = src[i];

    if (!(e & P_X86_PRESENT)) {
      if (level == 4) {
        uintptr_t child = alloc_table();
        if (!child)
          return -ENOMEM;

        dst[i] = child | P_X86_PRESENT | P_X86_WRITABLE | P_X86_OWNED;
      }
      continue;
    }

    if (level == 1 || (level < 4 && (e & P_X86_HUGE))) {

      dst[i] = e;
      continue;
    }

    uintptr_t child = alloc_table();
    if (!child)
      return -ENOMEM;

    int r = clone_level(p2v(child), table_of(e), level - 1);
    if (r)
      return r;

    dst[i] = child | (e & TABLE_FLAGS_MASK) | P_X86_OWNED;
  }
  return 0;
}

static bool set_leaf_protect(uint64_t *e, int level, int protect) {
  uint64_t keep =
      page_level_addr_mask(level) | PTE_ACCESSED | PTE_DIRTY | P_X86_OWNED;
  if (level > 1)
    keep |= P_X86_HUGE | PTE_PAT_HUGE;

  uint64_t nf = leaf_flags(protect);
  uint64_t old = atomic_load_explicit(e, memory_order_relaxed);
  uint64_t nw;

  do {
    nw = (old & keep) | nf;
    if (nw == old)
      return false;
  } while (!atomic_compare_exchange_strong_explicit(
      e, &old, nw, memory_order_acq_rel, memory_order_relaxed));

  return true;
}
page_table_t *kernel_pt_init() {
  assert(!s_kPtReady);

  uintptr_t pml4 = alloc_table();
  if (!pml4)
    panic("out of memory allocating pml4");

  uint64_t *src = p2v(read_cr3() & MASK_4K);

  int r = clone_level(p2v(pml4), src, 4);
  if (r != 0)
    panic("out of memory cloning boot page tables");

  s_kernelPt.pml4 = pml4;
  spinlock_init(&s_kernelPt.lock);
  s_kPtReady = true;

  return &s_kernelPt;
}

page_table_t *kernel_pt() {
  assert(s_kPtReady);
  return &s_kernelPt;
}

static int map_at_level(page_table_t *pt, uintptr_t virt, uintptr_t phys,
                        int protect, int level) {
  uint64_t size = level == 1   ? PAGE_SIZE
                  : level == 2 ? SUPERPAGE_SIZE
                               : MEGAPAGE_SIZE;

  if ((virt & (size - 1)) || (phys & (size - 1)))
    return -EINVAL;
  if (!page_is_canonical(virt) || (phys & ~MASK_4K))
    return -EINVAL;

  int r = validate_vmem_protect(protect);
  if (r)
    return r;

  bool user = !(protect & P_KERNEL);
  int err = 0;

  spin_lock(&pt->lock);

  uint64_t *e = walk_create(pt, virt, level, user, &err);
  if (!e) {
    spin_unlock(&pt->lock);
    return err;
  }

  if (*e & P_X86_PRESENT) {
    spin_unlock(&pt->lock);
    return -EEXIST;
  }

  uint64_t v = phys | leaf_flags(protect) | (level > 1 ? P_X86_HUGE : 0);

  atomic_store_explicit(e, v, memory_order_release);

  spin_unlock(&pt->lock);
  return 0;
}

int pt_map(page_table_t *pt, uintptr_t virt, uintptr_t phys, int protect) {
  return map_at_level(pt, virt, phys, protect, 1);
}

int pt_map2mb(page_table_t *pt, uintptr_t virt, uintptr_t phys, int protect) {
  return map_at_level(pt, virt, phys, protect, 2);
}

int pt_map1gb(page_table_t *pt, uintptr_t virt, uintptr_t phys, int protect) {
  static int supported = -1;
  if (supported < 0)
    supported = megapages_supported() ? 1 : 0;
  if (!supported)
    return -EINVAL;

  return map_at_level(pt, virt, phys, protect, 3);
}

int pt_unmap(page_table_t *pt, uintptr_t virt) {
  if (!page_is_canonical(virt))
    return -EINVAL;

  int level;

  spin_lock(&pt->lock);

  uint64_t *e = find_leaf(pt, virt, &level, false);
  if (!e) {
    spin_unlock(&pt->lock);
    return -ENOENT;
  }

  if (virt & ((pages_level(level) * PAGE_SIZE) - 1)) {
    spin_unlock(&pt->lock);
    return -EINVAL;
  }

  atomic_store_explicit(e, 0, memory_order_release);
  spin_unlock(&pt->lock);

  local_invlpg(virt);
  // tlb_flush(pt, virt, 1);
  return 0;
}

int pt_protect(page_table_t *pt, void *ptr, int protect) {
  return pt_protect_multiple(pt, ptr, 1, protect);
}

int pt_protect_multiple(page_table_t *pt, void *ptr, size_t pages,
                        int protect) {
  uintptr_t start = (uintptr_t)ptr;

  if (!pt)
    return -EINVAL;

  int r = validate_vmem_protect(protect);
  if (r)
    return r;

  if (pages == 0)
    return 0;

  if (start & (PAGE_SIZE - 1))
    return -EINVAL;

  if (pages > (UINTPTR_MAX / PAGE_SIZE) || start + pages * PAGE_SIZE < start)
    return -EINVAL;

  if (!page_is_canonical(start) ||
      !page_is_canonical(start + pages * PAGE_SIZE - 1))
    return -EINVAL;

  bool user = !(protect & P_KERNEL);
  int ret = 0;

  spin_lock(&pt->lock);

  {
    uintptr_t virt = start;
    size_t rem = pages;
    while (rem) {
      int level;
      uint64_t *e = find_leaf(pt, virt, &level, false);
      if (!e) {
        spin_unlock(&pt->lock);
        return -ENOENT;
      }
      size_t n = pages_chunk(virt, level, rem);
      virt += n * PAGE_SIZE;
      rem -= n;
    }
  }

  flush_batch_t batch = {.start = 0, .pages = 0};
  uintptr_t virt = start;
  size_t rem = pages;

  while (rem) {
    int level;
    uint64_t *e = find_leaf(pt, virt, &level, user);
    if (!e) {
      ret = -ENOENT;
      break;
    }

    size_t n = pages_chunk(virt, level, rem);

    if (level > 1 && n < pages_level(level)) {

      ret = split_leaf(e, level);
      if (ret)
        break;
      continue;
    }

    if (set_leaf_protect(e, level, protect))
      tlb_batch_add(&batch, virt, n);

    virt += n * PAGE_SIZE;
    rem -= n;
  }

  spin_unlock(&pt->lock);

  tlb_batch_flush(pt, &batch);
  return ret;
}

uintptr_t pt_v2p(page_table_t *pt, uintptr_t virt) {
  int level;

  spin_lock(&pt->lock);

  uint64_t *e = find_leaf(pt, virt, &level, false);
  uint64_t entry = e ? *e : 0;

  spin_unlock(&pt->lock);

  if (!e)
    return 0;

  uint64_t off_mask = (pages_level(level) * PAGE_SIZE) - 1;
  return (entry & page_level_addr_mask(level)) | (virt & off_mask);
}

void pt_switch(page_table_t *pt) {
  assert(pt);
  write_cr3(pt->pml4 & MASK_4K);
}

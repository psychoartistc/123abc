#include <arch/paging.h>
#include <mm/common.h>
#include <mm/physical/allocator.h>
#include <mm/virtual/page_table.h>
#include <mm/virtual/vma/vma.h>
#include <scheduler/spinlock.h>
#include <utils/logging.h>
#include <utils/panic.h>

static spinlock_t s_lock;
static vma_tree_t s_free;
static vma_tree_t s_used;
static vma_area_t *s_pool;
static bool s_ready;

#define IS_RED(n) ((n) && (n)->color == VMA_RBTREE_COL_RED)
#define IS_BLACK(n) (!(n) || (n)->color == VMA_RBTREE_COL_BLACK)

static inline void update(vma_area_t *n) {
  size_t m = n->size;
  if (n->left && n->left->max > m)
    m = n->left->max;
  if (n->right && n->right->max > m)
    m = n->right->max;
  n->max = m;
}

static void propagate(vma_area_t *n) {
  for (; n; n = n->parent)
    update(n);
}

static void rotate_left(vma_tree_t *t, vma_area_t *x) {
  vma_area_t *y = x->right;

  x->right = y->left;
  if (y->left)
    y->left->parent = x;

  y->parent = x->parent;
  if (!x->parent)
    t->root = y;
  else if (x == x->parent->left)
    x->parent->left = y;
  else
    x->parent->right = y;

  y->left = x;
  x->parent = y;

  update(x);
  update(y);
}

static void rotate_right(vma_tree_t *t, vma_area_t *x) {
  vma_area_t *y = x->left;

  x->left = y->right;
  if (y->right)
    y->right->parent = x;

  y->parent = x->parent;
  if (!x->parent)
    t->root = y;
  else if (x == x->parent->right)
    x->parent->right = y;
  else
    x->parent->left = y;

  y->right = x;
  x->parent = y;

  update(x);
  update(y);
}

static void insert_fixup(vma_tree_t *t, vma_area_t *z) {
  while (IS_RED(z->parent)) {
    vma_area_t *gp = z->parent->parent;

    if (z->parent == gp->left) {
      vma_area_t *u = gp->right;
      if (IS_RED(u)) {
        z->parent->color = VMA_RBTREE_COL_BLACK;
        u->color = VMA_RBTREE_COL_BLACK;
        gp->color = VMA_RBTREE_COL_RED;
        z = gp;
      } else {
        if (z == z->parent->right) {
          z = z->parent;
          rotate_left(t, z);
        }
        z->parent->color = VMA_RBTREE_COL_BLACK;
        gp = z->parent->parent;
        gp->color = VMA_RBTREE_COL_RED;
        rotate_right(t, gp);
      }
    } else {
      vma_area_t *u = gp->left;
      if (IS_RED(u)) {
        z->parent->color = VMA_RBTREE_COL_BLACK;
        u->color = VMA_RBTREE_COL_BLACK;
        gp->color = VMA_RBTREE_COL_RED;
        z = gp;
      } else {
        if (z == z->parent->left) {
          z = z->parent;
          rotate_right(t, z);
        }
        z->parent->color = VMA_RBTREE_COL_BLACK;
        gp = z->parent->parent;
        gp->color = VMA_RBTREE_COL_RED;
        rotate_left(t, gp);
      }
    }
  }
  t->root->color = VMA_RBTREE_COL_BLACK;
}

static void rb_insert(vma_tree_t *t, vma_area_t *z) {
  vma_area_t *y = NULL, *x = t->root;

  while (x) {
    y = x;
    x = z->start < x->start ? x->left : x->right;
  }

  z->parent = y;
  z->left = z->right = NULL;
  z->color = VMA_RBTREE_COL_RED;
  z->max = z->size;

  if (!y)
    t->root = z;
  else if (z->start < y->start)
    y->left = z;
  else
    y->right = z;

  propagate(y);
  insert_fixup(t, z);
}

static void transplant(vma_tree_t *t, vma_area_t *u, vma_area_t *v) {
  if (!u->parent)
    t->root = v;
  else if (u == u->parent->left)
    u->parent->left = v;
  else
    u->parent->right = v;
  if (v)
    v->parent = u->parent;
}

static void erase_fixup(vma_tree_t *t, vma_area_t *x, vma_area_t *xp) {
  while (x != t->root && IS_BLACK(x)) {
    if (x == xp->left) {
      vma_area_t *w = xp->right;
      if (IS_RED(w)) {
        w->color = VMA_RBTREE_COL_BLACK;
        xp->color = VMA_RBTREE_COL_RED;

        rotate_left(t, xp);
        w = xp->right;
      }
      if (IS_BLACK(w->left) && IS_BLACK(w->right)) {
        w->color = VMA_RBTREE_COL_RED;
        x = xp;
        xp = x->parent;
      } else {
        if (IS_BLACK(w->right)) {
          w->left->color = VMA_RBTREE_COL_BLACK;
          w->color = VMA_RBTREE_COL_RED;
          rotate_right(t, w);

          w = xp->right;
        }

        w->color = xp->color;
        xp->color = VMA_RBTREE_COL_BLACK;
        if (w->right)
          w->right->color = VMA_RBTREE_COL_BLACK;

        rotate_left(t, xp);
        x = t->root;
        xp = NULL;
      }
    } else {
      vma_area_t *w = xp->left;
      if (IS_RED(w)) {
        w->color = VMA_RBTREE_COL_BLACK;
        xp->color = VMA_RBTREE_COL_RED;
        rotate_right(t, xp);
        w = xp->left;
      }

      if (IS_BLACK(w->left) && IS_BLACK(w->right)) {
        w->color = VMA_RBTREE_COL_RED;
        x = xp;
        xp = x->parent;
      } else {
        if (IS_BLACK(w->left)) {
          w->right->color = VMA_RBTREE_COL_BLACK;
          w->color = VMA_RBTREE_COL_RED;
          rotate_left(t, w);
          w = xp->left;
        }
        w->color = xp->color;
        xp->color = VMA_RBTREE_COL_BLACK;
        if (w->left)
          w->left->color = VMA_RBTREE_COL_BLACK;
        rotate_right(t, xp);
        x = t->root;
        xp = NULL;
      }
    }
  }
  if (x)
    x->color = VMA_RBTREE_COL_BLACK;
}

static void rb_erase(vma_tree_t *t, vma_area_t *z) {
  vma_area_t *x, *xp;
  uint32_t removed_color = z->color;

  if (!z->left) {
    x = z->right;
    xp = z->parent;
    transplant(t, z, z->right);

  } else if (!z->right) {
    x = z->left;
    xp = z->parent;
    transplant(t, z, z->left);

  } else {
    vma_area_t *y = z->right;
    while (y->left)
      y = y->left;

    removed_color = y->color;
    x = y->right;

    if (y->parent == z) {
      xp = y;
    } else {
      xp = y->parent;
      transplant(t, y, y->right);
      y->right = z->right;
      y->right->parent = y;
    }

    transplant(t, z, y);
    y->left = z->left;
    y->left->parent = y;
    y->color = z->color;
  }

  propagate(xp);

  if (removed_color == VMA_RBTREE_COL_BLACK)
    erase_fixup(t, x, xp);

  z->parent = z->left = z->right = NULL;
}

static vma_area_t *rb_find(vma_tree_t *t, uintptr_t start) {
  vma_area_t *n = t->root;
  while (n) {
    if (start == n->start)
      return n;
    n = start < n->start ? n->left : n->right;
  }
  return NULL;
}

static void rb_neighbors(vma_tree_t *t, uintptr_t key, vma_area_t **pred,
                         vma_area_t **succ) {
  vma_area_t *n = t->root;
  *pred = *succ = NULL;
  while (n) {
    if (n->start < key) {
      *pred = n;
      n = n->right;
    } else {
      *succ = n;
      n = n->left;
    }
  }
}

static vma_area_t *find_first_fit(size_t need) {
  vma_area_t *n = s_free.root;

  if (!n || n->max < need)
    return NULL;

  for (;;) {
    if (n->left && n->left->max >= need)
      n = n->left;
    else if (n->size >= need)
      return n;
    else
      n = n->right;
  }
}

static vma_area_t *pool_pop(void) {
  vma_area_t *n = s_pool;
  if (n)
    s_pool = n->left;
  return n;
}

static void pool_push(vma_area_t *n) {
  n->left = s_pool;
  s_pool = n;
}

static bool pool_refill(void) {
  pmalloc_res_t r = pmalloc(1);
  if (!r.ok)
    return false;

  vma_area_t *nodes = p2v(r.addr);
  size_t n = PAGE_SIZE / sizeof(vma_area_t);

  for (size_t i = 0; i + 1 < n; i++)
    nodes[i].left = &nodes[i + 1];

  spin_lock(&s_lock);

  nodes[n - 1].left = s_pool;
  s_pool = &nodes[0];

  spin_unlock(&s_lock);
  return true;
}

static vma_area_t *reserve_range(size_t need) {
  for (;;) {
    spin_lock(&s_lock);

    vma_area_t *f = find_first_fit(need);
    if (!f) {
      spin_unlock(&s_lock);
      return NULL;
    }

    vma_area_t *u;
    if (f->size == need) {

      rb_erase(&s_free, f);
      u = f;
    } else {
      u = pool_pop();
      if (!u) {
        spin_unlock(&s_lock);
        if (!pool_refill())
          return NULL;

        continue;
      }
      u->start = f->start;
      f->start += need;
      f->size -= need;
      propagate(f);
    }

    u->size = need;
    rb_insert(&s_used, u);

    spin_unlock(&s_lock);
    return u;
  }
}

static void release_range(vma_area_t *a) {
  vma_area_t *pred, *succ;

  spin_lock(&s_lock);

  rb_neighbors(&s_free, a->start, &pred, &succ);

  bool merge_prev = pred && pred->start + pred->size == a->start;
  bool merge_next = succ && a->start + a->size == succ->start;

  if (merge_prev) {
    size_t add = a->size;
    if (merge_next) {
      add += succ->size;
      rb_erase(&s_free, succ);
      pool_push(succ);
    }
    pred->size += add;
    propagate(pred);
    pool_push(a);
  } else if (merge_next) {
    succ->start = a->start;
    succ->size += a->size;
    propagate(succ);
    pool_push(a);
  } else {
    rb_insert(&s_free, a);
  }

  spin_unlock(&s_lock);
}

static void zero_frame(uintptr_t phys) {
  volatile uint64_t *p = p2v(phys);
  for (size_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++)
    p[i] = 0;
}

static void unmap_and_free(uintptr_t virt, size_t pages) {
  page_table_t *pt = kernel_pt();
  for (size_t i = 0; i < pages; i++) {
    uintptr_t va = virt + i * PAGE_SIZE;
    uintptr_t pa = pt_v2p(pt, va);

    if (unlikely(!pa)) {
      pr_warn("[vma] no mapping at %p while freeing\n", (void *)va);
      continue;
    }

    if (unlikely(pt_unmap(pt, va) != 0)) {
      pr_warn("[vma] failed to unmap %p\n", (void *)va);
      continue;
    }

    pfree(pa & ~(uintptr_t)(PAGE_SIZE - 1), 1);
  }
}

void vma_init(void) {
  assert(!s_ready);

  spinlock_init(&s_lock);
  s_free.root = NULL;
  s_used.root = NULL;
  s_pool = NULL;

  if (!pool_refill())
    panic("[vma_init] out of memory for the vma node pool");

  spin_lock(&s_lock);
  vma_area_t *n = pool_pop();
  n->start = vmalloc_window_base();
  n->size = VMALLOC_WINDOW_SIZE;
  rb_insert(&s_free, n);
  spin_unlock(&s_lock);

  s_ready = true;
}

void *vmalloc(size_t size, int prot) {
  assert(s_ready);
  page_table_t *pt = kernel_pt();

  if (size == 0 || size > VMALLOC_WINDOW_SIZE - PAGE_SIZE)
    return NULL;

  size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
  size_t reserve = (pages + 1) * PAGE_SIZE;

  vma_area_t *area = reserve_range(reserve);
  if (!area)
    return NULL;

  prot |= P_KERNEL;

  size_t mapped = 0;
  for (; mapped < pages; mapped++) {
    uintptr_t va = area->start + mapped * PAGE_SIZE;

    pmalloc_res_t r = pmalloc(1);
    if (!r.ok)
      break;

    zero_frame(r.addr);

    if (pt_map(pt, va, r.addr, prot) != 0) {
      pfree(r.addr, 1);
      break;
    }
  }

  if (mapped != pages) {
    unmap_and_free(area->start, mapped);
    release_range(area);
    return NULL;
  }

  return (void *)area->start;
}

void vfree(const void *a) {
  assert(s_ready);

  if (!a)
    return;

  uintptr_t addr = (uintptr_t)a;
  uintptr_t base = vmalloc_window_base();

  if (addr < base || addr >= base + VMALLOC_WINDOW_SIZE) {
    pr_warn("[vfree] %p is outside the vmalloc window\n", a);
    return;
  }

  spin_lock(&s_lock);

  vma_area_t *area = rb_find(&s_used, addr);

  if (area)
    rb_erase(&s_used, area);

  spin_unlock(&s_lock);

  if (!area) {
    pr_warn("[vfree] %p is not the start of a vmalloc allocation\n", a);
    return;
  }

  unmap_and_free(area->start, area->size / PAGE_SIZE - 1);

  release_range(area);
}

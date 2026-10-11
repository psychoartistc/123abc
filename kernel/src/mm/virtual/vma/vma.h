#ifndef VMA_H_
#define VMA_H_

/*
 * vma.h
 * virtual memory allocator */

#include <stddef.h>
#include <stdint.h>

// 32tb
#define VMALLOC_WINDOW_SIZE (32ULL * 1024 * 1024 * 1024 * 1024)

#define VMA_RBTREE_COL_RED 0
#define VMA_RBTREE_COL_BLACK 1

typedef struct vma_area {
  struct vma_area *parent, *left, *right;
  uintptr_t start;
  size_t size;
  size_t max;
  uint32_t color;
} vma_area_t;

typedef struct {
  vma_area_t *root;
} vma_tree_t;

uintptr_t vmalloc_window_base();

void vma_init();
void *vmalloc(size_t size, int prot);
void vfree(const void *a);

#endif

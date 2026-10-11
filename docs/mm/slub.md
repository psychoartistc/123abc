# SLUB allocator
The default (and the only ) kernel memory allocator

## Terminology
There are three terms that will be used after this:  
* Cache - holds *slabs*  
* Slab  - big region of some amount of pages, these contain slots  
* Slot  - a memory region prefixed with a header that contains object's cache id and etc.  
A cache has a name and a global atomic id

## API and usage
```cpp
void kfree(void *ptr);
void *krealloc(void *ptr, size_t size);

void kmem_init();
void kmem_*();
void *kmalloc(size_t size);
void kmem_cache_*();
```  
Beforing using this, `kmem_init()` must be called, otherwise every other call will be undefined behaviour    

`kmalloc()` allocates a chunk of memory, if it is over 4096 bytes then the allocation goes to `vmalloc()`  
`kfree()` checks if the allocation actually happened, if the allocation is over 4096 and then chooses a path based off this  
`krealloc()` simply moves the memory

All of those functions are lock free and thread safe

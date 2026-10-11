#ifndef MEMORY_H
#define MEMORY_H

/*
 * memory.h
 * memory manipulation functions
 * */

#include <stddef.h>
#include <stdint.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
char *strstr(const char *haystack, const char *needle);

int strcmp(const char *s1, const char *s2)
    __attribute__((warning("use strlcmp instead")));

int strlcmp(const char *s1, size_t s1sz, const char *s2, size_t s2sz);
size_t strnlen(const char *s1, size_t fb);

size_t strlen(const char *s1) __attribute__((warning("use strnlen instead")));

#endif

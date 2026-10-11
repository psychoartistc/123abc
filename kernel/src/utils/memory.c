#include <stddef.h>
#include <stdint.h>
#include <utils/memory.h>

#ifdef memcpy
#undef memcpy
#endif

void *memcpy(void *dest, const void *src, size_t n) {
  if (dest == NULL || src == NULL || n == 0) {
    return dest;
  }

  char *d = (char *)dest;
  const char *s = (const char *)src;

  if (d > s && d < s + n) {
    d += n;
    s += n;
    while (n--) {
      *(--d) = *(--s);
    }
  } else {
    while (n--) {
      *d++ = *s++;
    }
  }

  return dest;
}

#ifdef memset
#undef memset
#endif
void *memset(void *s, int c, size_t n) {
  uint8_t *p = s;

  for (size_t i = 0; i < n; i++) {
    p[i] = (uint8_t)c;
  }

  return s;
}

#ifdef memmove
#undef memmove
#endif
void *memmove(void *dest, const void *src, size_t n) {

  if (dest == src || n == 0) {
    return dest;
  }

  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;

  if (d < s) {

    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else {

    for (size_t i = n; i > 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }

  return dest;
}

#ifdef memcmp
#undef memcmp
#endif
int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;

  int result = 0;
  int flag = 0;

  for (size_t i = 0; i < n; i++) {
    int diff = (p1[i] > p2[i]) - (p1[i] < p2[i]);

    result |= diff & (~flag & 1);
    flag |= diff;
  }

  return result;
}

char *strstr(const char *haystack, const char *needle) {

  if (*needle == '\0') {
    return (char *)haystack;
  }

  while (*haystack != '\0') {

    if (*haystack == *needle) {
      const char *h = haystack;
      const char *n = needle;

      while (*h != '\0' && *n != '\0' && *h == *n) {
        h++;
        n++;
      }

      if (*n == '\0') {
        return (char *)haystack;
      }
    }
    haystack++;
  }

  return NULL;
}
int strcmp(const char *s1, const char *s2) {

  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;

  while (*p1 && (*p1 == *p2)) {
    p1++;
    p2++;
  }

  return *p1 - *p2;
}

int strlcmp(const char *s1, size_t s1sz, const char *s2, size_t s2sz) {
  size_t i = 0;

  while (i < s1sz && i < s2sz) {
    unsigned char c1 = (unsigned char)s1[i];
    unsigned char c2 = (unsigned char)s2[i];

    if (c1 != c2 || c1 == '\0') {
      return (c1 < c2) ? -1 : 1;
    }

    i++;
  }

  if (i == s1sz && i == s2sz) {
    return 0;
  }

  if (i == s1sz) {
    return (s2[i] == '\0') ? 0 : -1;
  }

  return (s1[i] == '\0') ? 0 : 1;
}

size_t strlen(const char *s) {
  size_t length = 0;
  while (s[length] != '\0') {
    length++;
  }
  return length;
}

size_t strnlen(const char *s1, size_t fb) {
  size_t len = 0;

  while (len < fb && s1[len] != '\0') {
    len++;
  }

  return len;
}

#ifndef FMT_SIZE_H_
#define FMT_SIZE_H_

/*
 * fmt_size.h
 * fmt_size() function */

#include <stddef.h>
#include <stdint.h>

#define FMTSIZE_BUFSIZE ((sizeof(size_t) * 8 * 30103 + 99999) / 100000 + 2)

// formats some amount of bytes into a cool string
// 2200 becomes 2.2k
int fmt_size(char *to, size_t s);

#endif

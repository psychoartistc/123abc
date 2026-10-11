#ifndef CRYPTO_RNG_H_
#define CRYPTO_RNG_H_

#include <stddef.h>
#include <stdint.h>

void rng_init();
void rng_fill_bytes(char *buff, size_t amount, size_t buffSize);

#endif

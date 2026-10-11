#include <limine_requests.h>
#include <security/crypto/rng.h>
#include <stddef.h>
#include <stdint.h>
#include <utils/panic.h>

static const uint8_t domain[] = {'p', 'r', 'o', 't', 'e',
                                 'c', 't', 'e', 'd', '!'};

static void secure_bzero(void *ptr, size_t len) {
  volatile uint8_t *p = (volatile uint8_t *)ptr;

  while (len--) {
    *p++ = 0;
  }
}

typedef struct {
  uint32_t state[8];
  uint64_t bit_count;
  uint8_t buffer[64];
  size_t buffer_len;
} sha256_ctx;

static uint32_t rotr32(uint32_t x, unsigned n) {
  return (x >> n) | (x << (32u - n));
}

static uint32_t sha_ch(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (~x & z);
}

static uint32_t sha_maj(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t sha_bsig0(uint32_t x) {
  return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22);
}

static uint32_t sha_bsig1(uint32_t x) {
  return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25);
}

static uint32_t sha_ssig0(uint32_t x) {
  return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3);
}

static uint32_t sha_ssig1(uint32_t x) {
  return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10);
}

static uint32_t load32_be(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}

static void store32_be(uint8_t *p, uint32_t x) {
  p[0] = (uint8_t)(x >> 24);
  p[1] = (uint8_t)(x >> 16);
  p[2] = (uint8_t)(x >> 8);
  p[3] = (uint8_t)x;
}

static void sha256_transform(sha256_ctx *ctx, const uint8_t block[64]) {
  static const uint32_t k[64] = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
      0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
      0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
      0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
      0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
      0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
      0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
      0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
      0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
      0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

  uint32_t w[64];

  for (size_t i = 0; i < 16; ++i) {
    w[i] = load32_be(block + i * 4);
  }

  for (size_t i = 16; i < 64; ++i) {
    w[i] = sha_ssig1(w[i - 2]) + w[i - 7] + sha_ssig0(w[i - 15]) + w[i - 16];
  }

  uint32_t a = ctx->state[0];
  uint32_t b = ctx->state[1];
  uint32_t c = ctx->state[2];
  uint32_t d = ctx->state[3];
  uint32_t e = ctx->state[4];
  uint32_t f = ctx->state[5];
  uint32_t g = ctx->state[6];
  uint32_t h = ctx->state[7];

  for (size_t i = 0; i < 64; ++i) {
    uint32_t t1 = h + sha_bsig1(e) + sha_ch(e, f, g) + k[i] + w[i];

    uint32_t t2 = sha_bsig0(a) + sha_maj(a, b, c);

    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;

  secure_bzero(w, sizeof(w));
}

static void sha256_init(sha256_ctx *ctx) {
  ctx->state[0] = 0x6a09e667u;
  ctx->state[1] = 0xbb67ae85u;
  ctx->state[2] = 0x3c6ef372u;
  ctx->state[3] = 0xa54ff53au;
  ctx->state[4] = 0x510e527fu;
  ctx->state[5] = 0x9b05688cu;
  ctx->state[6] = 0x1f83d9abu;
  ctx->state[7] = 0x5be0cd19u;

  ctx->bit_count = 0;
  ctx->buffer_len = 0;

  secure_bzero(ctx->buffer, sizeof(ctx->buffer));
}

static void sha256_update(sha256_ctx *ctx, const void *data, size_t len) {
  const uint8_t *src = (const uint8_t *)data;

  ctx->bit_count += (uint64_t)len * 8u;

  while (len != 0) {
    size_t n = 64 - ctx->buffer_len;

    if (n > len) {
      n = len;
    }

    for (size_t i = 0; i < n; ++i) {
      ctx->buffer[ctx->buffer_len + i] = src[i];
    }

    ctx->buffer_len += n;
    src += n;
    len -= n;

    if (ctx->buffer_len == 64) {
      sha256_transform(ctx, ctx->buffer);
      ctx->buffer_len = 0;
    }
  }
}

static void sha256_final(sha256_ctx *ctx, uint8_t out[32]) {
  size_t i = ctx->buffer_len;

  ctx->buffer[i++] = 0x80;

  if (i > 56) {
    while (i < 64) {
      ctx->buffer[i++] = 0;
    }

    sha256_transform(ctx, ctx->buffer);
    i = 0;
  }

  while (i < 56) {
    ctx->buffer[i++] = 0;
  }

  uint64_t bits = ctx->bit_count;

  ctx->buffer[56] = (uint8_t)(bits >> 56);
  ctx->buffer[57] = (uint8_t)(bits >> 48);
  ctx->buffer[58] = (uint8_t)(bits >> 40);
  ctx->buffer[59] = (uint8_t)(bits >> 32);
  ctx->buffer[60] = (uint8_t)(bits >> 24);
  ctx->buffer[61] = (uint8_t)(bits >> 16);
  ctx->buffer[62] = (uint8_t)(bits >> 8);
  ctx->buffer[63] = (uint8_t)bits;

  sha256_transform(ctx, ctx->buffer);

  for (size_t j = 0; j < 8; ++j) {
    store32_be(out + j * 4, ctx->state[j]);
  }

  secure_bzero(ctx, sizeof(*ctx));
}

static void sha256_update_u64_le(sha256_ctx *ctx, uint64_t value) {
  uint8_t bytes[8];

  bytes[0] = (uint8_t)(value);
  bytes[1] = (uint8_t)(value >> 8);
  bytes[2] = (uint8_t)(value >> 16);
  bytes[3] = (uint8_t)(value >> 24);
  bytes[4] = (uint8_t)(value >> 32);
  bytes[5] = (uint8_t)(value >> 40);
  bytes[6] = (uint8_t)(value >> 48);
  bytes[7] = (uint8_t)(value >> 56);

  sha256_update(ctx, bytes, sizeof(bytes));

  secure_bzero(bytes, sizeof(bytes));
}

typedef struct {
  uint32_t state[16];
  uint8_t initialized;
} rng_state;

static rng_state rng;

static uint32_t rotl32(uint32_t x, unsigned n) {
  return (x << n) | (x >> (32u - n));
}

static uint32_t load32_le(const uint8_t *p) {
  return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static void chacha_quarter_round(uint32_t *a, uint32_t *b, uint32_t *c,
                                 uint32_t *d) {
  *a += *b;
  *d ^= *a;
  *d = rotl32(*d, 16);

  *c += *d;
  *b ^= *c;
  *b = rotl32(*b, 12);

  *a += *b;
  *d ^= *a;
  *d = rotl32(*d, 8);

  *c += *d;
  *b ^= *c;
  *b = rotl32(*b, 7);
}

static void chacha20_block(const uint32_t in[16], uint8_t out[64]) {
  uint32_t x[16];

  for (size_t i = 0; i < 16; ++i) {
    x[i] = in[i];
  }

  for (size_t i = 0; i < 10; ++i) {

    chacha_quarter_round(&x[0], &x[4], &x[8], &x[12]);
    chacha_quarter_round(&x[1], &x[5], &x[9], &x[13]);
    chacha_quarter_round(&x[2], &x[6], &x[10], &x[14]);
    chacha_quarter_round(&x[3], &x[7], &x[11], &x[15]);

    chacha_quarter_round(&x[0], &x[5], &x[10], &x[15]);
    chacha_quarter_round(&x[1], &x[6], &x[11], &x[12]);
    chacha_quarter_round(&x[2], &x[7], &x[8], &x[13]);
    chacha_quarter_round(&x[3], &x[4], &x[9], &x[14]);
  }

  for (size_t i = 0; i < 16; ++i) {
    uint32_t v = x[i] + in[i];

    out[i * 4 + 0] = (uint8_t)v;
    out[i * 4 + 1] = (uint8_t)(v >> 8);
    out[i * 4 + 2] = (uint8_t)(v >> 16);
    out[i * 4 + 3] = (uint8_t)(v >> 24);
  }

  secure_bzero(x, sizeof(x));
}

void rng_init(void) {

  if (entropy_request.response == NULL ||
      entropy_request.response->values == NULL ||
      entropy_request.response->value_count == 0) {
    rng.initialized = 0;
    panic("no limine entropy");
    return;
  }

  sha256_ctx hash;
  uint8_t digest[32];

  sha256_init(&hash);

  sha256_update(&hash, domain, sizeof(domain));

  for (size_t i = 0; i < entropy_request.response->value_count; ++i) {
    uint64_t val = entropy_request.response->values[i];

    sha256_update_u64_le(&hash, val);
  }

  sha256_final(&hash, digest);

  rng.state[0] = 0x61707865u;
  rng.state[1] = 0x3320646eu;
  rng.state[2] = 0x79622d32u;
  rng.state[3] = 0x6b206574u;

  for (size_t i = 0; i < 8; ++i) {
    rng.state[4 + i] = load32_le(digest + i * 4);
  }

  rng.state[12] = 0;
  rng.state[13] = 0;

  rng.state[14] = 0;
  rng.state[15] = 0;

  rng.initialized = 1;

  secure_bzero(digest, sizeof(digest));
}

static void rng_rekey(void) {
  uint8_t material[64];
  uint8_t new_key[32];
  sha256_ctx hash;

  chacha20_block(rng.state, material);

  sha256_init(&hash);

  sha256_update(&hash, domain, sizeof(domain));
  sha256_update(&hash, material, sizeof(material));
  sha256_final(&hash, new_key);

  for (size_t i = 0; i < 8; ++i) {
    rng.state[4 + i] = load32_le(new_key + i * 4);
  }

  rng.state[12] = 0;
  rng.state[13] = 0;

  secure_bzero(material, sizeof(material));
  secure_bzero(new_key, sizeof(new_key));
}
void rng_fill_bytes(char *buff, size_t amount, size_t buffSize) {
  if (amount == 0) {
    return;
  }

  if (!rng.initialized) {
    panic("rng not initialized");
    return;
  }

  if (buff == NULL || amount > buffSize) {
    panic("out of bounds");
    return;
  }

  uint8_t block[64];
  uint8_t *dst = (uint8_t *)buff;

  while (amount != 0) {
    chacha20_block(rng.state, block);

    ++rng.state[12];

    if (rng.state[12] == 0) {
      ++rng.state[13];
    }

    size_t n = amount < sizeof(block) ? amount : sizeof(block);

    for (size_t i = 0; i < n; ++i) {
      dst[i] = block[i];
    }

    dst += n;
    amount -= n;

    secure_bzero(block, sizeof(block));
  }

  rng_rekey();

  secure_bzero(block, sizeof(block));
}

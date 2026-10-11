#ifndef PSF_H_
#define PSF_H_

/*
 * psf.h
 * psf font data */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <utils/attributes.h>
#include <utils/int.h>

#define PSF1_MAGIC 0x0436
#define PSF2_MAGIC 0x864AB572

typedef struct psf1_header {
  u16 magic;
  u8 mode;
  u8 char_size;
} __PACKED psf1_header_t;

typedef struct psf2_header {
  u32 magic;
  u32 version;
  u32 header_size;
  u32 flags;
  u32 numglyph;
  u32 bytes_per_glyph;
  u32 height;
  u32 width;
} __PACKED psf2_header_t;

typedef struct psf_font {
  int version;
  u32 width;
  u32 height;
  u32 numglyph;
  u32 bytes_per_glyph;
  u8 *glyphs;
  bool parsed;
} psf_font_t;

struct psf_font psf_parse(const u8 *buffer);
u8 *psf_get_glyph(const psf_font_t *font, size_t index);

#endif

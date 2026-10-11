#include <utils/branching.h>
#include <utils/psf.h>

struct psf_font psf_parse(const u8 *buffer) {
  if (unlikely(!buffer))
    goto failure;

  uint16_t magic1 = *(uint16_t *)buffer;
  if (magic1 == PSF1_MAGIC) {
    psf1_header_t *hdr = (psf1_header_t *)buffer;

    return (psf_font_t){.version = 1,
                        .width = 8,
                        .height = hdr->char_size,
                        .numglyph = (hdr->mode & 0x01) ? 512 : 256,
                        .bytes_per_glyph = hdr->char_size,
                        .glyphs = (u8 *)(buffer + sizeof(psf1_header_t)),
                        .parsed = true};
  }

  uint32_t magic2 = *(uint32_t *)buffer;
  if (magic2 == PSF2_MAGIC) {
    psf2_header_t *hdr = (psf2_header_t *)buffer;
    return (psf_font_t){.version = 2,
                        .width = hdr->width,
                        .height = hdr->height,
                        .numglyph = hdr->numglyph,
                        .bytes_per_glyph = hdr->bytes_per_glyph,
                        .glyphs = (u8 *)(buffer + hdr->header_size),
                        .parsed = true};
  }

failure:
  return (psf_font_t){.parsed = false};
}

uint8_t *psf_get_glyph(const psf_font_t *font, size_t index) {
  if (!font || index >= font->numglyph)
    return NULL;

  return font->glyphs + index * font->bytes_per_glyph;
}


#ifdef BGRT_ENABLE
#include <drivers/api.h>
#include <hardware/acpi/parse.h>
#include <limine_requests.h>
#include <mm/common.h>
#include <mm/virtual/page_table.h>
#include <stddef.h>
#include <utils/errno.h>
#include <utils/logging.h>

typedef struct {
  acpi_header_t header;
  uint16_t version;
  uint8_t status;
  uint8_t image_type;
  uint64_t image_address;
  uint32_t image_offset_x;
  uint32_t image_offset_y;
} __PACKED acpi_bgrt_t;

static uint16_t read_le16(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static bool framebuffer_valid(const struct limine_framebuffer *fb) {
  if (fb == NULL || fb->address == NULL)
    return false;

  if (fb->memory_model != LIMINE_FRAMEBUFFER_RGB)
    return false;

  if (fb->bpp != 24 && fb->bpp != 32)
    return false;

  uint8_t sizes[] = {fb->red_mask_size, fb->green_mask_size,
                     fb->blue_mask_size};

  uint8_t shifts[] = {fb->red_mask_shift, fb->green_mask_shift,
                      fb->blue_mask_shift};

  for (unsigned i = 0; i < 3; ++i) {
    if (sizes[i] == 0 || sizes[i] > 8)
      return false;

    if ((unsigned)shifts[i] + sizes[i] > fb->bpp)
      return false;
  }

  uint64_t bytes_per_pixel = fb->bpp / 8;

  if (fb->width > UINT64_MAX / bytes_per_pixel)
    return false;

  if (fb->pitch < fb->width * bytes_per_pixel)
    return false;

  return fb->height != 0;
}

static uint32_t scale_channel(uint8_t value, uint8_t bits) {
  uint32_t max_value = (1u << bits) - 1u;

  return ((uint32_t)value * max_value + 127u) / 255u;
}

static void put_pixel(struct limine_framebuffer *fb, uint64_t x, uint64_t y,
                      uint8_t red, uint8_t green, uint8_t blue) {
  if (x >= fb->width || y >= fb->height)
    return;

  uint32_t pixel =
      (scale_channel(red, fb->red_mask_size) << fb->red_mask_shift) |
      (scale_channel(green, fb->green_mask_size) << fb->green_mask_shift) |
      (scale_channel(blue, fb->blue_mask_size) << fb->blue_mask_shift);

  uint8_t *dst = (uint8_t *)fb->address + y * fb->pitch + x * (fb->bpp / 8);

  for (unsigned i = 0; i < fb->bpp / 8; ++i)
    dst[i] = (uint8_t)(pixel >> (8 * i));
}

static bool draw_bmp(struct limine_framebuffer *fb, const uint8_t *bmp,
                     uint32_t available_size, uint64_t origin_x,
                     uint64_t origin_y, unsigned rotation) {
  if (!framebuffer_valid(fb) || bmp == NULL)
    return false;

  if (available_size < 54)
    return false;

  if (bmp[0] != 'B' || bmp[1] != 'M')
    return false;

  uint32_t file_size = read_le32(bmp + 2);
  uint32_t pixel_offset = read_le32(bmp + 10);
  uint32_t dib_size = read_le32(bmp + 14);

  if (file_size < 54 || file_size > available_size)
    return false;

  if (dib_size < 40 || dib_size > file_size - 14)
    return false;

  if (pixel_offset < 14 + dib_size || pixel_offset > file_size)
    return false;

  int32_t signed_width = (int32_t)read_le32(bmp + 18);
  int32_t signed_height = (int32_t)read_le32(bmp + 22);

  uint16_t planes = read_le16(bmp + 26);
  uint16_t bpp = read_le16(bmp + 28);
  uint32_t compression = read_le32(bmp + 30);

  if (signed_width <= 0 || signed_height == 0 || signed_height == INT32_MIN)
    return false;

  if (planes != 1 || (bpp != 24 && bpp != 32) || compression != 0)
    return false;

  uint64_t width = (uint32_t)signed_width;
  uint64_t height = signed_height < 0 ? (uint64_t)(-(int64_t)signed_height)
                                      : (uint32_t)signed_height;

  uint64_t row_bits = width * bpp;
  uint64_t row_stride = ((row_bits + 31) / 32) * 4;
  uint64_t data_size = file_size - pixel_offset;

  if (row_stride == 0 || height > data_size / row_stride)
    return false;

  const uint8_t *pixels = bmp + pixel_offset;
  unsigned src_bytes = bpp / 8;

  for (uint64_t sy = 0; sy < height; ++sy) {
    uint64_t row_index = signed_height > 0 ? height - 1 - sy : sy;

    const uint8_t *row = pixels + row_index * row_stride;

    for (uint64_t sx = 0; sx < width; ++sx) {
      const uint8_t *p = row + sx * src_bytes;

      uint8_t blue = p[0];
      uint8_t green = p[1];
      uint8_t red = p[2];

      uint64_t dx, dy;

      switch (rotation & 3u) {
      case 0:
        dx = sx;
        dy = sy;
        break;

      case 1:
        dx = height - 1 - sy;
        dy = sx;
        break;

      case 2:
        dx = width - 1 - sx;
        dy = height - 1 - sy;
        break;

      default:
        dx = sy;
        dy = width - 1 - sx;
        break;
      }

      uint64_t screen_x = origin_x + dx;
      uint64_t screen_y = origin_y + dy;

      if (screen_x >= fb->width || screen_y >= fb->height)
        continue;

      put_pixel(fb, screen_x, screen_y, red, green, blue);
    }
  }

  return true;
}

void bgrt_show(const acpi_bgrt_t *bgrt) {
  if (!framebuffer_available())
    return;

  // framebuffer_request->
  if (bgrt->header.length < sizeof(*bgrt))
    return;

  if (bgrt->version > 1)
    return;

  if (!(bgrt->status & 1u))
    return;

  if (bgrt->image_type != 0 || bgrt->image_address == 0)
    return;

  page_table_t *pt = kernel_pt();

  pt_map(pt, (uintptr_t)p2v(bgrt->image_address), bgrt->image_address,
         P_READ | P_WRITE);

  const u8 *bmp = (u8 *)p2v(bgrt->image_address);
  uint32_t bmp_size = read_le32(bmp + 2);

  if (bmp_size < 54)
    return;

  unsigned rotation = (bgrt->status >> 1) & 3u;
  struct limine_framebuffer_response *resp = framebuffer_request.response;

  for (size_t i = 0; i < resp->framebuffer_count; i++) {
    struct limine_framebuffer *fb = resp->framebuffers[i];
    if (!framebuffer_valid(fb))
      continue;

    draw_bmp(fb, bmp, bmp_size, bgrt->image_offset_x, bgrt->image_offset_y,
             rotation);
  }
}

static int bgrt_on_parse(acpi_header_t *header) {
  pr_info("[bgrt] showing manufacturer image");
  bgrt_show((const acpi_bgrt_t *)header);
  return 0;
}

static int bgrt_parser_entry() {
  acpi_add_parser(
      (acpi_parser_t){.tbl_signature = "BGRT", .on_parse = bgrt_on_parse});

  return 0;
}

REGISTER_DRIVER(bgrt_parser, "bgrt acpi table parser",
                "parses the bgrt acpi table and draws the image", "1.0.0",
                DRVLICENSE_DEFAULT, DRV_INIT_ORDER_ACPI_PARSER,
                bgrt_parser_entry, NULL);

#else
void bgrt_show() {}
#endif

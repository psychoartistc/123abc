#include <drivers/api.h>
#include <drivers/misc/fterm/backends/fb.h>
#include <drivers/misc/fterm/flanterm.h>
#include <hardware/acpi/bgrt.h>
#include <limine_requests.h>
#include <mm/kern/slub.h>
#include <utils/errno.h>
#include <utils/logging.h>
#include <utils/memory.h>
#include <utils/printf.h>
#include <utils/psf.h>

#define FLANTERM_FONT_PATH "zap-vga16.psf"
#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define BG_COL 0x000000

static inline void ft_free_wrapper(void *ptr, size_t size) {
  (void)size;
  kfree(ptr);
}

static struct flanterm_context *s_ftCtx = NULL;

struct flanterm_log_color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static const struct flanterm_log_color s_logColors[8] = {
    [(L_TRACE)] = {0xb0, 0xb0, 0xb0},    [(L_NOTE)] = {0x8a, 0x8a, 0x8a},
    [(L_DEBUG)] = {0x6f, 0xc7, 0xd9},    [(L_INFO)] = {0x9f, 0xc4, 0x5f},
    [(L_WARN)] = {0xe0, 0xb4, 0x5c},     [(L_ERROR)] = {0xe0, 0x6c, 0x6c},
    [(L_CRITICAL)] = {0xff, 0x55, 0x55}, [(L_PRINTK)] = {0xff, 0xff, 0xff},
};

static void ft_write(const char *str) {
  if (!s_ftCtx)
    return;

  flanterm_write(s_ftCtx, str, strnlen(str, 512));
}

static void ft_write_color(const struct flanterm_log_color *color) {
  char buf[32];

  int len = snprintf_(buf, sizeof(buf), "\033[38;2;%u;%u;%um", color->r,
                      color->g, color->b);

  if (len > 0)
    flanterm_write(s_ftCtx, buf, (size_t)len);
}

static void ft_putchar(char c, void *a) {
  (void)a;

  if (!s_ftCtx)
    return;

  if (c == '\n') {
    const char *crlf = "\r\n";
    flanterm_write(s_ftCtx, crlf, 2);
    return;
  }

  flanterm_write(s_ftCtx, &c, 1);
}

#ifdef BGRT_ENABLE
static void ft_callback(struct flanterm_context *ctx, uint64_t type,
                        uint64_t arg1, uint64_t arg2, uint64_t arg3) {
  if (type == FLANTERM_CB_DEC) {
    bgrt_show();
  }
}
#endif

static void ft_log_poller(u8 level, const char *fmt, va_list va) {
  if (!s_ftCtx)
    return;

  if (level != L_PRINTK) {
    const struct flanterm_log_color *color = &s_logColors[level];
    ft_write_color(color);
    ft_write(ANSI_BOLD);
    fctprintf(ft_putchar, NULL, "[ %s ] %s", logs_level_str(level), ANSI_RESET);
  }
  vfctprintf(ft_putchar, NULL, fmt, va);
  if (level != L_PRINTK)
    ft_putchar('\n', NULL);
}

static int flanterm_entry() {
// find FLANTERM_FONT_PATH
#ifndef FLANTERM_USE_DEFAULT_FONT
  bool has_font = false;
  psf_font_t font = {0};
  if (module_request.response) {
    struct limine_module_response *resp = module_request.response;
    for (size_t i = 0; i < resp->module_count; i++) {
      struct limine_file *file = resp->modules[i];
      if (unlikely(!file))
        continue;

      pr_info("%s", file->path);

      if (strstr(file->path, FLANTERM_FONT_PATH) != NULL) {
        pr_info("font\n\n\n\n\n");
        font = psf_parse(file->address);
        if (font.parsed)
          has_font = true;
      }
    }
  }
#endif
  u32 bg_col = BG_COL;

  if (framebuffer_available()) {
    struct limine_framebuffer *fb =
        framebuffer_request.response->framebuffers[0];
#ifndef FLANTERM_USE_DEFAULT_FONT
    if (has_font) {
      pr_info("[flanterm] using font %s", FLANTERM_FONT_PATH);
      s_ftCtx = flanterm_fb_init(
          kmalloc, ft_free_wrapper, fb->address, fb->width, fb->height,
          fb->pitch, fb->red_mask_size, fb->red_mask_shift, fb->green_mask_size,
          fb->green_mask_shift, fb->blue_mask_size, fb->blue_mask_shift, NULL,
          NULL, NULL, &bg_col, NULL, NULL, NULL, font.glyphs, font.width,
          font.height, 0, 1, 1, 0, 0, true);
    } else {
#endif
      s_ftCtx = flanterm_fb_init(
          kmalloc, ft_free_wrapper, fb->address, fb->width, fb->height,
          fb->pitch, fb->red_mask_size, fb->red_mask_shift, fb->green_mask_size,
          fb->green_mask_shift, fb->blue_mask_size, fb->blue_mask_shift, NULL,
          NULL, NULL, &bg_col, NULL, NULL, NULL, NULL, 0, 0, 1, 0, 0, 0, 0,
          true);
#ifndef FLANTERM_USE_DEFAULT_FONT
    }
#endif
    if (unlikely(!s_ftCtx)) {
      pr_error("[flanterm] failed to create flanterm context");
      return -ENOENT;
    }
#ifdef BGRT_ENABLE
    flanterm_set_callback(s_ftCtx, ft_callback);
#endif
    logs_add_handler(ft_log_poller);
  }

  return 0;
}

REGISTER_DRIVER(flanterm_driver, "flanterm",
                "handles logs and polls them to flanterm", "1.0.0",
                DRVLICENSE_DEFAULT, DRV_INIT_ORDER_NORMAL, flanterm_entry,
                NULL);

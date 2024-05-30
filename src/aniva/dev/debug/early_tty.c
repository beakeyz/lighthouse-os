#include "early_tty.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "libk/gfx/font.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"

/*
 * Aniva early debug tty
 *
 * This is a framebuffer-based 'console' (Not even, there is no userinput possible) that catches every type of
 * system log. This is enabled standard and can be disabled either in the bootloader (TODO) or by the user (TODO)
 */

struct simple_char {
  char c;
  uint32_t clr;
};

static struct multiboot_tag_framebuffer* framebuffer;
static uintptr_t vid_buffer;
static uintptr_t y_idx;
static uintptr_t x_idx;
static uint32_t char_xres;
static uint32_t char_yres;
static aniva_font_t* etty_font;
static size_t _char_list_size;
static struct simple_char* _char_list;

static uint32_t WHITE = 0xFFFFFFFF;
static uint32_t BLACK = 0x00000000;

static int etty_putch(char c);
static int etty_print(const char* str);
static int etty_println(const char* str);

logger_t early_logger = {
  .title = "early_tty",
  .flags = LOGGER_FLAG_INFO | LOGGER_FLAG_DEBUG | LOGGER_FLAG_WARNINGS,
  .f_log = etty_print,
  .f_logln = etty_println,
  .f_logc = etty_putch,
};

static ALWAYS_INLINE void __etty_pixel(uint32_t x, uint32_t y, uint32_t clr)
{
  /* Unsafe poke into video memory */
  *(uint32_t volatile*)(vid_buffer + framebuffer->common.framebuffer_pitch * y + x * framebuffer->common.framebuffer_bpp / 8) = clr;
}

static inline struct simple_char* get_simple_char_at(uint32_t x, uint32_t y)
{
  return &_char_list[y * char_xres + x];
}

static int etty_draw_char_ex(uint32_t x, uint32_t y, char c, uint32_t clr, bool force_update)
{
  struct simple_char* s_char;
  uint32_t char_xstart, char_ystart;
  uint8_t* glyph;

  s_char = get_simple_char_at(x, y);

  /* If we force this update then it's all good */
  if (s_char->c == c && s_char->clr == clr && !force_update)
    return 0;

  char_xstart = x * etty_font->width;
  char_ystart = y * etty_font->height;

  if (get_glyph_for_char(etty_font, c, &glyph))
    return -1;

  s_char->clr = clr;
  s_char->c = c;

  for (uint32_t i = 0; i < etty_font->height; i++) {
    for (uint32_t j = 0; j < etty_font->width; j++) {
      if (glyph[i] & (1 << j))
        __etty_pixel(char_xstart + j, char_ystart + i, clr);
      else 
        __etty_pixel(char_xstart + j, char_ystart + i, BLACK);
    }
  }

  return 0;
}

static inline int etty_draw_char(uint32_t x, uint32_t y, char c, uint32_t clr)
{
  return etty_draw_char_ex(x, y, c, clr, false);
}

static inline void etty_clear()
{
  for (uint32_t y = 0; y < char_yres; y++) {
    for (uint32_t x = 0; x < char_xres; x++) {
      etty_draw_char_ex(x, y, NULL, BLACK, true);
    }
  }
}

void init_early_tty()
{
  struct multiboot_tag_framebuffer* fb;

  get_default_font(&etty_font);

  fb = g_system_info.firmware_fb;

  framebuffer = fb;
  y_idx = 0;
  x_idx = 0;

  char_xres = fb->common.framebuffer_width / etty_font->width;
  char_yres = fb->common.framebuffer_height / etty_font->height;

  vid_buffer = Must(__kmem_kernel_alloc(fb->common.framebuffer_addr, fb->common.framebuffer_pitch * fb->common.framebuffer_height, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  KLOG_DBG("Framebuffer: 0x%llx\n", vid_buffer);

  /*
   * Allocate a range for our characters
   */
  _char_list_size = char_xres * char_yres * sizeof(struct simple_char);
  _char_list = (void*)Must(__kmem_kernel_alloc_range(_char_list_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  memset(_char_list, 0, _char_list_size);

  etty_clear();

  /* Make sure we're installed */
  register_logger(&early_logger);

  g_system_info.sys_flags |= SYSFLAGS_HAS_EARLY_TTY;
}

void destroy_early_tty()
{
  if ((g_system_info.sys_flags & SYSFLAGS_HAS_EARLY_TTY) != SYSFLAGS_HAS_EARLY_TTY)
    return;

  unregister_logger(&early_logger);

  etty_clear();

  g_system_info.sys_flags &= ~SYSFLAGS_HAS_EARLY_TTY;

  /* Deallocate the char list */
  __kmem_kernel_dealloc((vaddr_t)_char_list, _char_list_size);
}

static void etty_scroll(uint32_t lines)
{
  struct simple_char* c_char;

  if (!lines)
    return;

  for (uint32_t y = lines; y < char_yres; y++) {
    for (uint32_t x = 0; x < char_xres; x++) {
      c_char = get_simple_char_at(x, y);

      etty_draw_char_ex(x, y-lines, c_char->c, c_char->clr, false);
    }
  }

  for (uint32_t y = (char_yres - lines); y < char_yres; y++)
    for (uint32_t x = 0; x < char_xres; x++)
      etty_draw_char_ex(x, y, NULL, BLACK, false);

  y_idx -= lines;
}

static int _etty_shift_x_idx()
{
  x_idx++;
  
  if (x_idx >= char_xres) {
    x_idx = 0;
    y_idx++;
  }

  if (y_idx >= char_yres) {
    etty_scroll(1);
  }

  return 0;
}

int etty_putch(char c)
{
  if (c == '\n') {
    x_idx = 0;
    y_idx++;

    if (y_idx >= char_yres)
      etty_scroll(1);
  } else {
    etty_draw_char(x_idx, y_idx, c, WHITE);
    _etty_shift_x_idx();
  }

  return 0;
}

int etty_print(const char* str)
{
  while (*str) {

    if (*str == '\n') {
      x_idx = 0;
      y_idx++;

      if (y_idx >= char_yres)
        etty_scroll(1);

      goto next;
    }

    etty_draw_char(x_idx, y_idx, *str, WHITE);

    _etty_shift_x_idx();

next:
    str++;
  }

  return 0;
}

int etty_println(const char* str)
{
  etty_print(str);
  return etty_print("\n");
}

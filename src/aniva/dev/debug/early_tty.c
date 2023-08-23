#include "early_tty.h"
#include "dev/debug/serial.h"
#include "entry/entry.h"
#include "libk/gfx/font.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"

static struct multiboot_tag_framebuffer* framebuffer;
static uintptr_t vid_buffer;
static uintptr_t y_idx;
static uintptr_t x_idx;
static aniva_font_t* etty_font;

static uint32_t WHITE = 0xFFFFFFFF;
static uint32_t BLACK = 0x00000000;

static void __etty_pixel(uint32_t x, uint32_t y, uint32_t clr)
{
  /* Unsafe poke into video memory */
  *(uint32_t volatile*)(vid_buffer + framebuffer->common.framebuffer_pitch * y + x * framebuffer->common.framebuffer_bpp / 8) = clr;
}

void init_early_tty(struct multiboot_tag_framebuffer* fb)
{
  get_default_font(&etty_font);

  framebuffer = fb;
  y_idx = 0;
  x_idx = 0;

  println(to_string((uint64_t)fb));
  println(to_string((uint64_t)fb->common.framebuffer_addr));

  vid_buffer = Must(__kmem_kernel_alloc(fb->common.framebuffer_addr, fb->common.framebuffer_pitch * fb->common.framebuffer_height, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  for (uint32_t y = 0; y < framebuffer->common.framebuffer_height; y++) {
    for (uint32_t x = 0; x < framebuffer->common.framebuffer_width; x++) {
      __etty_pixel(x, y, BLACK);
    }
  }

  g_system_info.sys_flags |= SYSFLAGS_HAS_EARLY_TTY;
}

void destroy_early_tty()
{
  for (uint32_t y = 0; y < framebuffer->common.framebuffer_height; y++) {
    for (uint32_t x = 0; x < framebuffer->common.framebuffer_width; x++) {
      __etty_pixel(x, y, BLACK);
    }
  }

  g_system_info.sys_flags &= ~SYSFLAGS_HAS_EARLY_TTY;
}

void etty_print(char* str)
{
  int error;
  uint8_t* glyph;

  while (*str) {

    if (*str == '\n') {
      x_idx = 0;
      y_idx += etty_font->height;
      goto next;
    }

    error = get_glyph_for_char(etty_font, *str, &glyph);

    if (error)
      return;

    for (uintptr_t y = 0; y < etty_font->height; y++) {
      for (uintptr_t x = 0; x < etty_font->width; x++) {
        if (glyph[y] & (1 << x))
          __etty_pixel(x_idx + x, y_idx + y, WHITE);
        else 
          __etty_pixel(x_idx + x, y_idx + y, BLACK);
      }
    }

    x_idx += etty_font->width;
next:
    str++;
  }
}

void etty_println(char* str)
{
  etty_print(str);
  etty_print("\n");
}

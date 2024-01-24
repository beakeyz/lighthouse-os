#include "wallpaper.h"
#include "dev/video/framebuffer.h"
#include "drivers/lwnd/screen.h"
#include "drivers/lwnd/window.h"
#include "libk/data/bitmap.h"
#include "libk/string.h"
#include "logging/log.h"

static int lwnd_wallpaper_draw(lwnd_window_t* window) 
{
  uint32_t pixels;

  if (!window->fb_ptr)
    return -1;

  pixels = window->fb_size / sizeof(fb_color_t);

  /* Set the entire screen to white */
  for (uint32_t i = 0; i < pixels; i++) {
    *((fb_color_t*)window->fb_ptr + i) = (fb_color_t) {
      .components = {
        .r = 0xff,
        .g = 0x00,
        .b = 0xff,
        .a = 0xff,
      },
    };
  }

  return 0;
}

lwnd_window_ops_t lwnd_wallpaper_ops = {
  .f_draw = lwnd_wallpaper_draw,
  .f_update = nullptr,
};

void lwnd_set_wallpaper_ops(lwnd_window_t* window)
{
  (void)lwnd_window_set_ops(window, &lwnd_wallpaper_ops);
}


void init_lwnd_wallpaper(lwnd_screen_t* screen)
{
  lwnd_window_t* wnd;

  wnd = create_lwnd_window(screen, 0, 0, screen->width, screen->height, LWND_WNDW_NEVER_MOVE | LWND_WNDW_NEVER_FOCUS, LWND_TYPE_LWND_OWNED, nullptr);

  if (!wnd)
    return;

  lwnd_request_framebuffer(wnd);
  lwnd_set_wallpaper_ops(wnd);
}

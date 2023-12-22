
/*
 * This is a demo app that should utilise the compositor and the 
 * graphics API to draw a smily face on a canvas of 75x75
 */
#include "LibGfx/include/driver.h"
#include "LibGfx/include/lgfx.h"
#include "LibGfx/include/video.h"
#include "sys/types.h"

lwindow_t window;
lframebuffer_t fb;

int main() 
{
  BOOL res;

  /* Initialize the graphics API */

  /* Connect to the compositor (Create a blank window of 75x75) */
  res = request_lwindow(&window, 75, 75, NULL);

  if (!res)
    return -1;

  res = lwindow_request_framebuffer(&window, &fb);

  if (!res)
    return -2;

  /* Draw the smily */
  lwindow_draw_rect(&window, 10, 10, 10, 10, RGBA(0xA5, 0x00, 0, 0xFF));
  lwindow_draw_rect(&window, 50, 10, 10, 10, RGBA(0xA5, 0x00, 0, 0xFF));

  lwindow_draw_rect(&window, 10, 30, 10, 10, RGBA(0xA5, 0x00, 0, 0xFF));
  lwindow_draw_rect(&window, 50, 30, 10, 10, RGBA(0xA5, 0x00, 0, 0xFF));

  lwindow_draw_rect(&window, 15, 35, 40, 10, RGBA(0xA5, 0x00, 0, 0xFF));

  /* Yield */
  while (true) {
  }

  close_lwindow(&window);
  return 0;
}
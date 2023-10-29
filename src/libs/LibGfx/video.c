#include "include/video.h"
#include "LibGfx/include/driver.h"
#include "LibSys/driver/drv.h"

BOOL lwindow_request_framebuffer(lwindow_t* wnd, lframebuffer_t* fb)
{
  BOOL res;
  
  if (!wnd || !fb)
    return FALSE;

  res = driver_send_msg(wnd->lwnd_handle, LWND_DCC_REQ_FB, wnd, sizeof(*wnd));

  if (!res)
    return FALSE;

  *fb = wnd->fb;
  return TRUE;
}

BOOL lwindow_draw_rect(lwindow_t* wnd, uint32_t x, uint32_t y, uint32_t width, uint32_t height, lcolor_t clr)
{
  if (!wnd || !wnd->fb)
    return FALSE;

  for (uint32_t i = 0; i < height; i++) {
    for (uint32_t j = 0; j < width; j++) {
      *((lframebuffer_t)wnd->fb + (y + i) * wnd->current_width + (x + j)) = clr;
    }
  }

  return driver_send_msg(wnd->lwnd_handle, LWND_DCC_UPDATE_WND, wnd, sizeof(*wnd));
}

#include "include/video.h"
#include "LibGfx/include/driver.h"
#include "lightos/driver/drv.h"

VOID lwindow_set_updates_deferred(lwindow_t* wnd)
{
  wnd->wnd_flags |= LWND_FLAG_DEFER_UPDATE;
}

VOID lwindow_set_updates_immediate(lwindow_t* wnd)
{
  wnd->wnd_flags &= ~LWND_FLAG_DEFER_UPDATE;
}

BOOL lwindow_update(lwindow_t* wnd)
{
  return driver_send_msg(wnd->lwnd_handle, LWND_DCC_UPDATE_WND, wnd, sizeof(*wnd));
}

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

  if ((wnd->wnd_flags & LWND_FLAG_DEFER_UPDATE) == LWND_FLAG_DEFER_UPDATE)
    return TRUE;

  return lwindow_update(wnd);
}

BOOL lwindow_draw_buffer(lwindow_t* wnd, uint32_t startx, uint32_t starty, uint32_t width, uint32_t height, lcolor_t* buffer)
{
  if (!wnd || !wnd->fb)
    return FALSE;

  for (uint32_t i = 0; i < height; i++) {
    for (uint32_t j = 0; j < width; j++) {
      *((lframebuffer_t)wnd->fb + (starty + i) * wnd->current_width + (startx + j)) = *buffer;

      buffer++;
    }
  }

  if ((wnd->wnd_flags & LWND_FLAG_DEFER_UPDATE) == LWND_FLAG_DEFER_UPDATE)
    return TRUE;

  return lwindow_update(wnd);
}

#include "include/lgfx.h"
#include "LibGfx/include/driver.h"
#include "LibSys/driver/drv.h"
#include "LibSys/handle.h"
#include "LibSys/handle_def.h"
#include <string.h>

BOOL request_lwindow(lwindow_t* wnd, DWORD width, DWORD height, DWORD flags)
{
  BOOL res;

  if (!wnd || !width || !height)
    return FALSE;

  memset(wnd, 0, sizeof(*wnd));

  /* Prepare our window object */
  wnd->current_height = height;
  wnd->current_width = width;
  wnd->wnd_flags = flags;

  /* Open lwnd */
  res = open_driver(LWND_DRV_PATH, NULL, NULL, &wnd->lwnd_handle);

  if (!res)
    return FALSE;

  /* Send it a message that we want to make a window */
  return driver_send_msg(wnd->lwnd_handle, LWND_DCC_CREATE, wnd, sizeof(*wnd));
}

BOOL close_lwindow(lwindow_t* wnd)
{
  BOOL res;
  
  if (!wnd)
    return FALSE;
  
  res = driver_send_msg(wnd->lwnd_handle, LWND_DCC_CLOSE, wnd, sizeof(*wnd));

  if (!res)
    return FALSE;

  /* Make sure the handle to the driver gets closed here */
  return close_handle(wnd->lwnd_handle);
}

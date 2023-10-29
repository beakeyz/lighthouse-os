#ifndef __LIGHTENV_LIBGFX_DRIVER__
#define __LIGHTENV_LIBGFX_DRIVER__

/*
 * Driver controlcodes to interact with the windowing driver
 */
#include "LibSys/handle_def.h"
#include <LibC/stdint.h>

#define LWND_DCC_CREATE 10
#define LWND_DCC_CLOSE 11
#define LWND_DCC_MINIMIZE 12
#define LWND_DCC_RESIZE 13
#define LWND_DCC_REQ_FB 14
#define LWND_DCC_UPDATE_WND 15

/*
 * Userspace window flags
 */

/* Should this window have a top bar */
#define LWND_FLAG_NO_BAR 0x00000001;
#define LWND_FLAG_NO_CLOSE_BTN 0x00000002;
#define LWND_FLAG_NO_HIDE_BTN 0x00000004;
#define LWND_FLAG_NO_FULLSCREEN_BTN 0x00000008;

/*
 * One process can have multiple windows?
 */
typedef struct lwindow {
  HANDLE lwnd_handle;
  uint32_t wnd_id;
  uint32_t wnd_flags;
  uint32_t current_width;
  uint32_t current_height;

  void* fb;
} lwindow_t;

#endif // !__LIGHTENV_LIBGFX_DRIVER__

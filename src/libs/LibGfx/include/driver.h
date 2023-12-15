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
#define LWND_DCC_GETKEY 16

#define LKEY_MOD_LALT 0x0001
#define LKEY_MOD_RALT 0x0002
#define LKEY_MOD_LCTL 0x0004
#define LKEY_MOD_RCTL 0x0008
#define LKEY_MOD_LSHIFT 0x0010
#define LKEY_MOD_RSHIFT 0x0020
#define LKEY_MOD_SUPER 0x0040

/*
 * Structure for keyevents under lwnd
 */
typedef struct lkey_event {
  bool pressed;
  uint8_t pressed_char;
  uint16_t mod_flags;
  uint32_t keycode;
} lkey_event_t;

/*
 * Userspace window flags
 */

/* Should this window have a top bar */
#define LWND_FLAG_NO_BAR 0x00000001;
#define LWND_FLAG_NO_CLOSE_BTN 0x00000002;
#define LWND_FLAG_NO_HIDE_BTN 0x00000004;
#define LWND_FLAG_NO_FULLSCREEN_BTN 0x00000008;

#define LWND_DEFAULT_EVENTBUFFER_CAPACITY 512

/*
 * One process can have multiple windows?
 */
typedef struct lwindow {
  HANDLE lwnd_handle;
  /* Handle to the key event */
  HANDLE event_handle;
  uint32_t wnd_id;
  uint32_t wnd_flags;
  uint32_t current_width;
  uint32_t current_height;

  uint32_t keyevent_buffer_capacity;
  uint32_t keyevent_buffer_write_idx;
  uint32_t keyevent_buffer_read_idx;
  lkey_event_t* keyevent_buffer;

  void* fb;
} lwindow_t;

#endif // !__LIGHTENV_LIBGFX_DRIVER__

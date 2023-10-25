#ifndef __ANIVA_LWND_WINDOW__
#define __ANIVA_LWND_WINDOW__

#include "proc/proc.h"
#include <libk/stddef.h>
#include <dev/manifest.h>

/* Limit to how many windows one client can have: 255 */
typedef uint8_t window_id_t;
typedef uint8_t window_type_t;

#define LWND_TYPE_PROCESS 0x0
#define LWND_TYPE_INFO 0x1
#define LWND_TYPE_ERROR 0x2
#define LWND_TYPE_DRIVER 0x3
#define LWND_TYPE_SYSTEM 0x4
/* Things like taskbar, menus, etc. that are managed by lwnd */
#define LWND_TYPE_LWND_OWNED 0x5

/*
 * Kernel-side structure representing a window on the screen
 */
typedef struct lwnd_window {
  window_id_t id;
  window_type_t type;
  uint32_t flags;

  /* Dimentions */
  uint32_t width;
  uint32_t height;

  /* Positions */
  uint32_t x;
  uint32_t y;

  /*
   * More things than only processes may open
   * windows
   */
  union {
    proc_t* proc;
    dev_manifest_t* driver;
  } client;

  /* TODO: better framebuffer management */
  void* fb_ptr;
  size_t fb_size;
} lwnd_window_t;

#define LWND_WNDW_HIDDEN    0x00000001
#define LWND_WNDW_MAXIMIZED 0x00000002
#define LWND_WNDW_LOCKED_CURSOR 0x00000004
#define LWND_WNDW_GPU_RENDERED 0x00000008
/* Does this window have a close button? */
#define LWND_WNDW_CLOSE_BTN 0x00000010
/* Does this window have a fullscreen button? */
#define LWND_WNDW_FULLSCREEN_BTN 0x00000020
/* Does this window have a hide button? */
#define LWND_WNDW_HIDE_BTN 0x00000040
#define LWND_WNDW_DRAGGING 0x00000080
#define LWND_WNDW_FOCUSSED 0x00000080

#endif // !__ANIVA_LWND_WINDOW__

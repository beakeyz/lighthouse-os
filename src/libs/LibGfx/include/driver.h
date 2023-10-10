#ifndef __LIGHTENV_LIBGFX_DRIVER__
#define __LIGHTENV_LIBGFX_DRIVER__

/*
 * Driver controlcodes to interact with the windowing driver
 */
#include <LibC/stdint.h>

#define LWND_DCC_CREATE 10
#define LWND_DCC_CLOSE 11
#define LWND_DCC_MINIMIZE 12
#define LWND_DCC_RESIZE 13

/*
 * One process can have multiple windows?
 */
typedef struct lwindow {
  uint32_t wnd_id;
  uint32_t current_width;
  uint32_t current_height;
} lwindow_t;

#endif // !__LIGHTENV_LIBGFX_DRIVER__

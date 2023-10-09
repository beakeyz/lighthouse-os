#ifndef __LIGHTLIB_LGFX__
#define __LIGHTLIB_LGFX__

/*
 * The Light GFX graphics library
 *
 * This is the userspace implementation for the graphics stack under Aniva. We work with graphics drivers
 * (providers) to supply us with framebuffers, processing, and ultimately drawing stuff. Here, the user can 
 * initialize a graphics environment to work in, but there can only be one active environment on the system.
 * The driver is in charge of managing these environments, so we ask the driver if it can hook us into its
 * system and then we recieve our environment. This is in the form of a context buffer, in which we can do 
 * lots of funny things
 *
 */

/* These are the driver control codes implemented by lwnd. This library makes use of these
 * but userprocesses can also directly interract with lwnd.drv through this
 */
#include "LibSys/handle_def.h"
#include "sys/types.h"
#include <LibSys/handle.h>

#define LWND_DCC_CREATE 10
#define LWND_DCC_CLOSE 11
#define LWND_DCC_MINIMIZE 12

/*
 * One process can have multiple windows?
 */
typedef struct lwindow {
  DWORD wnd_id;
  DWORD current_width;
  DWORD current_height;
} lwindow_t;

/*
 * The window manager will keep track of which window comes from which
 * process by looking at who the request is comming from
 */
BOOL
request_window(lwindow_t* wnd, DWORD width, DWORD height, DWORD flags);

BOOL
close_window(lwindow_t* wnd);

#endif // !__LIGHTLIB_LGFX__

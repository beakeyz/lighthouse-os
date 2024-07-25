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
#include "shared.h"
#include <sys/types.h>

#define LWND_DRV_PATH_VAR "DFLT_LWND_PATH"

/*!
 * @brief: Ask the kernel about the value of the variable LWND_DRV_PATH_VAR on the Global profile
 */
extern BOOL get_lwnd_drv_path(char* buffer, size_t bufsize);

/*
 * The window manager will keep track of which window comes from which
 * process by looking at who the request is comming from
 */
extern BOOL request_lwindow(lwindow_t* p_wnd, const char* title, DWORD width, DWORD height, DWORD flags);
extern BOOL close_lwindow(lwindow_t* wnd);

#endif // !__LIGHTLIB_LGFX__

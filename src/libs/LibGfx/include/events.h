#ifndef __LIGHTENV_LIBGFX_EVENTS__
#define __LIGHTENV_LIBGFX_EVENTS__

/*
 * Ask our GFX provider for events
 */

#include "LibGfx/include/driver.h"
#include "sys/types.h"

BOOL get_key_event(lwindow_t* wnd, lkey_event_t* keyevent);

#endif // !__LIGHTENV_LIBGFX_EVENTS__

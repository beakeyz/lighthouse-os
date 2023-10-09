#ifndef __ANIVA_LWND_SCREEN__
#define __ANIVA_LWND_SCREEN__

/*
 * This struct represents a single physical screen on which pixels
 * will be beamed into our eyesockets
 */
#include "drivers/wnd/window.h"
#include "libk/data/linkedlist.h"
#include <libk/stddef.h>

typedef struct lwnd_screen {
  uint32_t count;
  lwnd_window_t* window_stack[];
} lwnd_screen_t;

#endif // !__ANIVA_LWND_SCREEN__

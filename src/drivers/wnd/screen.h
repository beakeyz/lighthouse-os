#ifndef __ANIVA_LWND_SCREEN__
#define __ANIVA_LWND_SCREEN__

/*
 * This struct represents a single physical screen on which pixels
 * will be beamed into our eyesockets
 */
#include "drivers/wnd/window.h"
#include "libk/data/bitmap.h"
#include "libk/data/linkedlist.h"

#include <libk/stddef.h>

/*
 * A physical screen
 *
 * Windows are layered in terms of z-index
 * with 0 being the top and N being the bottom
 *
 * When drawing to the framebuffer
 */
typedef struct lwnd_screen {
  uint32_t count;
  /* This bitmap is cleared on every render pass */
  bitmap_t* pixel_bitmap;
  lwnd_window_t* window_stack[];
} lwnd_screen_t;

#endif // !__ANIVA_LWND_SCREEN__

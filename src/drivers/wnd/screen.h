#ifndef __ANIVA_LWND_SCREEN__
#define __ANIVA_LWND_SCREEN__

/*
 * This struct represents a single physical screen on which pixels
 * will be beamed into our eyesockets
 */
#include "dev/video/framebuffer.h"
#include "drivers/wnd/event.h"
#include "drivers/wnd/io.h"
#include "drivers/wnd/window.h"
#include "libk/data/bitmap.h"
#include "libk/data/linkedlist.h"
#include "sync/mutex.h"

#include <libk/stddef.h>

#define SCREEN_FLAG_HAS_MOUSE   0x00000001
#define SCREEN_FLAG_IS_MAIN     0x00000002
#define SCREEN_FLAG_HAS_ALPHA   0x00000004

#define LWND_SCREEN_MAX_WND_COUNT 2048

/*
 * A physical screen
 *
 * Windows are layered in terms of z-index
 * with 0 being the top and N being the bottom
 *
 * When drawing to the framebuffer
 */
typedef struct lwnd_screen {
  uint16_t window_count;
  uint16_t max_window_count;
  uint32_t flags;
  uint32_t width;
  uint32_t height;
  size_t window_stack_size;

  lwnd_mouse_t* mouse;
  fb_info_t* info;

  mutex_t* event_lock;
  mutex_t* draw_lock;

  lwnd_event_t* events;

  /* This bitmap is cleared on every render pass */
  bitmap_t* pixel_bitmap;
  /* We consider every kind of 'widget' on the screen a window. This means that even the taskbar is a 'window' =) */
  lwnd_window_t** window_stack;
} lwnd_screen_t;

lwnd_screen_t* create_lwnd_screen(fb_info_t* fb, uint16_t max_wnd_count);

int lwnd_screen_register_window(lwnd_screen_t* screen, lwnd_window_t* window);
int lwnd_screen_draw_window(lwnd_window_t* window);

/*!
 * @brief: Grabs a window by its ID
 *
 * Returns null if the id is invalid
 */
static inline lwnd_window_t* lwnd_screen_get_window(lwnd_screen_t* screen, window_id_t id)
{
  if (id >= screen->window_count)
    return nullptr;

  return screen->window_stack[id];
}

static inline lwnd_window_t* lwnd_screen_get_top_window(lwnd_screen_t* screen)
{
  /* The windows that's at the 'top' of the window stack is always focussed */
  return (screen->window_stack[screen->window_count-1]);
}

int lwnd_screen_add_event(lwnd_screen_t* screen, lwnd_event_t* event);
lwnd_event_t* lwnd_screen_take_event(lwnd_screen_t* screen);

void lwnd_screen_handle_event(lwnd_screen_t* screen, lwnd_event_t* event);
void lwnd_screen_handle_events(lwnd_screen_t* screen);

#endif // !__ANIVA_LWND_SCREEN__

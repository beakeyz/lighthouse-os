#ifndef __ANIVA_LWND_SCREEN__
#define __ANIVA_LWND_SCREEN__

/*
 * This struct represents a single physical screen on which pixels
 * will be beamed into our eyesockets
 */
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/event.h"
#include "drivers/env/lwnd/io.h"
#include "drivers/env/lwnd/props/prop.h"
#include "drivers/env/lwnd/window.h"
#include "libk/data/bitmap.h"
#include "sync/mutex.h"

#include <libk/stddef.h>

#define SCREEN_FLAG_HAS_MOUSE 0x00000001
#define SCREEN_FLAG_IS_MAIN 0x00000002
#define SCREEN_FLAG_HAS_ALPHA 0x00000004

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

    uint32_t highest_wnd_idx;

    lwnd_mouse_t* mouse;
    fb_info_t* info;

    mutex_t* event_lock;
    mutex_t* draw_lock;

    lwnd_event_t* events;

    /* Null if there is no fullscreen window */
    lwnd_window_t* current_fullscreen;
    /* All the props on a screen will simply be linked together */
    lwnd_dynamic_prop_t* props;

    /* This bitmap is cleared on every render pass */
    bitmap_t* pixel_bitmap;
    bitmap_t* uid_bitmap;
    /* We consider every kind of 'widget' on the screen a window. This means that even the taskbar is a 'window' =) */
    lwnd_window_t** window_stack;
} lwnd_screen_t;

lwnd_screen_t* create_lwnd_screen(fb_info_t* fb, uint16_t max_wnd_count);

int lwnd_screen_register_window(lwnd_screen_t* screen, lwnd_window_t* window);
int lwnd_screen_unregister_window(lwnd_screen_t* screen, lwnd_window_t* window);
int lwnd_screen_draw_window(lwnd_window_t* window);

/*!
 * @brief: Check if this window is on the top of the z-stack
 */
static inline bool lwnd_window_is_top_window(lwnd_window_t* window)
{
    return window->stack_idx == (window->screen->window_count - 1);
}

static inline bool lwnd_window_has_valid_index(lwnd_window_t* window)
{
    if (!window || !window->screen)
        return false;

    return (window->stack_idx < window->screen->highest_wnd_idx && window == window->screen->window_stack[window->stack_idx]);
}

/*!
 * @brief: Grabs a window by its ID
 *
 * Returns null if the id is invalid
 */
static inline lwnd_window_t* lwnd_screen_get_window(lwnd_screen_t* screen, window_id_t id)
{
    for (window_id_t i = 0; i < screen->highest_wnd_idx; i++)
        if (screen->window_stack[i] && screen->window_stack[i]->uid == id)
            return screen->window_stack[i];

    return nullptr;
}

static inline lwnd_window_t* lwnd_screen_get_top_window(lwnd_screen_t* screen)
{
    /* The windows that's at the 'top' of the window stack is always focussed */
    return (screen->window_stack[screen->highest_wnd_idx - 1]);
}

lwnd_window_t* lwnd_screen_get_window_by_label(lwnd_screen_t* screen, const char* label);

int lwnd_screen_add_event(lwnd_screen_t* screen, lwnd_event_t* event);
lwnd_event_t* lwnd_screen_take_event(lwnd_screen_t* screen);

void lwnd_screen_handle_event(lwnd_screen_t* screen, lwnd_event_t* event);
void lwnd_screen_handle_events(lwnd_screen_t* screen);

#endif // !__ANIVA_LWND_SCREEN__

#ifndef __LWND_WINDOWING_WINDOW__
#define __LWND_WINDOWING_WINDOW__

#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "libk/stddef.h"
#include "mem/zalloc/zalloc.h"
#include "proc/proc.h"

#define LWND_WINDOW_FLAG_NEED_UPDATE 0x00000001

struct lwnd_wndstack;
struct lwnd_screen;

/*
 * Rectangle part of a window
 *
 * Used to break the window into smaller parts that need to be
 * updated, when there is overlap between multiple windows
 */
typedef struct lwnd_wndrect {
    /* These are relative to the window */
    u32 x;
    u32 y;
    u32 w;
    struct {
        bool rect_changed : 1;
        u32 h : 31;
    };

    struct lwnd_wndrect* next_part;
} lwnd_wndrect_t;

typedef struct lwnd_window {
    const char* title;

    u32 x, y;
    u32 width, height;

    /* Index this window has inside it's window stack */
    i32 stack_idx;
    u32 flags;

    /*
     * The process this window is for.
     * Might be null, which indicates lwnd owns this
     */
    proc_t* proc;

    /* Small cache to store the local rectancles of this window */
    zone_allocator_t* rect_cache;

    /* The window under this window xD */
    struct lwnd_window* next_layer;
    struct lwnd_window* prev_layer;

    /* Local framebuffer info */
    fb_info_t* this_fb;

    /*
     * The different rectangles that make up this window
     * Null if the window us unbroken
     */
    lwnd_wndrect_t* rects;
    lwnd_wndrect_t* prev_rects;
} lwnd_window_t;

static inline bool lwnd_window_should_update(lwnd_window_t* wnd)
{
    return ((wnd->flags & LWND_WINDOW_FLAG_NEED_UPDATE) == LWND_WINDOW_FLAG_NEED_UPDATE);
}

static inline bool lwnd_window_is_inside_window(lwnd_window_t* top, lwnd_window_t* bottom)
{
    return (top->x >= bottom->x && top->y >= bottom->y && (top->x + top->width) <= (bottom->x + bottom->width) && (top->y + top->height) <= (bottom->y + bottom->height));
}

lwnd_window_t* create_window(proc_t* p, const char* title, u32 x, u32 y, u32 width, u32 height);
void destroy_window(lwnd_window_t* window);

void lwnd_window_update(lwnd_window_t* wnd);
void lwnd_window_full_update(lwnd_window_t* wnd);
void lwnd_window_clear_update(lwnd_window_t* wnd);

int lwnd_window_move(lwnd_window_t* wnd, u32 newx, u32 newy);
int lwnd_window_request_fb(lwnd_window_t* wnd, struct lwnd_screen* screen);
int lwnd_window_draw(lwnd_window_t* wnd, struct lwnd_screen* screen);

extern lwnd_wndrect_t* create_and_link_lwndrect(lwnd_wndrect_t** rect_list, zone_allocator_t* cache, u32 x, u32 y, u32 w, u32 h);
// Uses fb_info or debug
extern int lwnd_window_split(struct lwnd_wndstack* stack, lwnd_window_t* wnd, bool front_to_back);

#endif // !__LWND_WINDOWING_WINDOW__

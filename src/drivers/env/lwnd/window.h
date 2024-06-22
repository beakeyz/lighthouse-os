#ifndef __ANIVA_LWND_WINDOW__
#define __ANIVA_LWND_WINDOW__

#include "dev/io/hid/event.h"
#include "dev/video/framebuffer.h"
#include "proc/proc.h"
#include "sync/mutex.h"
#include <dev/manifest.h>
#include <libk/stddef.h>

struct lwnd_screen;
struct lwnd_window_ops;

/* Limit to how many windows one client can have: 255 */
typedef uint16_t window_id_t;
typedef uint16_t window_type_t;

#define LWND_INVALID_ID 0xFFFF

#define LWND_TYPE_PROCESS 0x0
#define LWND_TYPE_INFO 0x1
#define LWND_TYPE_ERROR 0x2
#define LWND_TYPE_DRIVER 0x3
#define LWND_TYPE_SYSTEM 0x4
/* Things like taskbar, menus, etc. that are managed by lwnd */
#define LWND_TYPE_LWND_OWNED 0x5

#define LWND_WINDOW_KEYBUFFER_CAPACITY 32

/*
 * Kernel-side structure representing a window on the screen
 */
typedef struct lwnd_window {
    window_id_t uid;
    window_id_t stack_idx;
    window_type_t type;
    uint32_t flags;

    /* Dimentions */
    uint32_t width;
    uint32_t height;

    /* Positions */
    uint32_t x;
    uint32_t y;

    struct lwnd_screen* screen;
    struct lwnd_window_ops* ops;

    /* Keypress ringbuffer thingy */
    hid_event_t key_buffer[LWND_WINDOW_KEYBUFFER_CAPACITY];
    uint32_t key_buffer_write_idx;
    uint32_t key_buffer_read_idx;

    mutex_t* lock;

    const char* label;

    /*
     * More things than only processes may open
     * windows
     */
    union {
        proc_t* proc;
        drv_manifest_t* driver;
        void* raw;
    } client;

    /* TODO: better framebuffer management */
    void* user_real_fb_ptr;
    void* user_fb_ptr;
    void* fb_ptr;
    size_t fb_size;
} lwnd_window_t;

#define LWND_WNDW_HIDDEN 0x00000001
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
#define LWND_WNDW_NEVER_FOCUS 0x00000100
#define LWND_WNDW_NEVER_MOVE 0x00000200
#define LWND_WNDW_NEEDS_SYNC 0x00000400
#define LWND_WNDW_NEEDS_REPAINT 0x00000800

lwnd_window_t* create_lwnd_window(struct lwnd_screen* screen, uint32_t startx, uint32_t starty, uint32_t width, uint32_t height, uint32_t flags, window_type_t type, void* client);
void destroy_lwnd_window(lwnd_window_t* window);

int lwnd_request_framebuffer(lwnd_window_t* window);

static inline int lwnd_window_set_ops(lwnd_window_t* window, struct lwnd_window_ops* ops)
{
    if (!window)
        return -1;

    if (window && window->ops && ops)
        return -1;

    window->ops = ops;
    return 0;
}

typedef struct lwnd_window_ops {
    int (*f_draw)(lwnd_window_t* window);
    int (*f_update)(lwnd_window_t* window);
} lwnd_window_ops_t;

int lwnd_save_keyevent(lwnd_window_t* window, hid_event_t* ctx);
int lwnd_load_keyevent(lwnd_window_t* window, hid_event_t* ctx);

int lwnd_draw(lwnd_window_t* window);
int lwnd_clear(lwnd_window_t* window);

int lwnd_window_move(lwnd_window_t* window, uint32_t new_x, uint32_t new_y);
int lwnd_window_resize(lwnd_window_t* window, uint32_t new_width, uint32_t new_height);
int lwnd_window_focus(lwnd_window_t* window);
int lwnd_window_update(lwnd_window_t* window, bool should_clear);

static inline bool lwnd_window_should_redraw(lwnd_window_t* window)
{
    return ((window->flags & LWND_WNDW_NEEDS_SYNC) == LWND_WNDW_NEEDS_SYNC || (window->flags & LWND_WNDW_NEEDS_REPAINT) == LWND_WNDW_NEEDS_REPAINT);
}

static inline fb_color_t* get_color_at(lwnd_window_t* window, uint32_t x, uint32_t y)
{
    if (!window->fb_ptr)
        return nullptr;

    return ((fb_color_t*)window->fb_ptr + (y * window->width + x));
}

#endif // !__ANIVA_LWND_WINDOW__

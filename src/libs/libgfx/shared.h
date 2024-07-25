#ifndef __LIGHTENV_LIBGFX_DRIVER__
#define __LIGHTENV_LIBGFX_DRIVER__

/*
 * Driver controlcodes to interact with the windowing driver
 */
#include "lightos/handle_def.h"
#include <stdint.h>

#define LWND_DCC_CREATE 10
#define LWND_DCC_CLOSE 11
#define LWND_DCC_MINIMIZE 12
#define LWND_DCC_RESIZE 13
#define LWND_DCC_REQ_FB 14
#define LWND_DCC_UPDATE_WND 15
#define LWND_DCC_GETKEY 16

#define LKEY_MOD_LALT 0x0001
#define LKEY_MOD_RALT 0x0002
#define LKEY_MOD_LCTL 0x0004
#define LKEY_MOD_RCTL 0x0008
#define LKEY_MOD_LSHIFT 0x0010
#define LKEY_MOD_RSHIFT 0x0020
#define LKEY_MOD_SUPER 0x0040

/*
 * Structure for keyevents under lwnd
 */
typedef struct lkey_event {
    bool pressed;
    uint8_t pressed_char;
    uint16_t mod_flags;
    uint32_t keycode;
} lkey_event_t;

/*
 * User-side framebuffer
 *
 * Filled by the window management driver when the framebuffer is requested.
 * contains instructions on how to format colors and the 64-bit address to
 * our beautiful buffer =)
 */
typedef struct lframebuffer {
    uint8_t red_lshift;
    uint8_t green_lshift;
    uint8_t blue_lshift;
    uint8_t alpha_lshift;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;

    /* Bytes per scanline ((bpp >> 3) * width) */
    uint32_t pitch;
    uint32_t height;
    /* bits per pixel */
    uint32_t bpp;

    uintptr_t fb;
} lframebuffer_t;

typedef struct lclr_buffer {
    uint32_t width;
    uint32_t height;

    void* buffer;
} lclr_buffer_t;

/*
 * Userspace window flags
 */
#define LWND_FLAG_HAS_FB 0x000000001
#define LWND_FLAG_NEED_UPDATE 0x80000000

#define LWND_DEFAULT_EVENTBUFFER_CAPACITY 512

/*
 * One process can have multiple windows?
 */
typedef struct lwindow {
    HANDLE lwnd_handle;
    /* Handle to the key event */
    HANDLE event_handle;

    /* Windows have a unique title, which acts as a identifier */
    const char* title;

    uint32_t current_width;
    uint32_t current_height;

    uint32_t wnd_flags;
    uint32_t keyevent_buffer_capacity;
    uint32_t keyevent_buffer_write_idx;
    uint32_t keyevent_buffer_read_idx;
    lkey_event_t* keyevent_buffer;

    lframebuffer_t fb;
} lwindow_t;

#endif // !__LIGHTENV_LIBGFX_DRIVER__

#ifndef __ANIVA_KEVENT_KEYBOARD_TYPE__
#define __ANIVA_KEVENT_KEYBOARD_TYPE__
#include "kevent/event.h"
#include "lightos/event/key.h"
#include <libk/stddef.h>

typedef struct kevent_kb_ctx {
    /* Is the current key pressed or released? */
    uint8_t pressed : 1;
    /* Keycode that was typed (TODO: Refactor name to enum ANIVA_SCANCODES)*/
    uint32_t keycode;
    /* Can be null. Depends on the underlying driver */
    uint8_t pressed_char;
    /*
     * All the keys that are currently pressed
     * with pressed_keys[0] being the first and
     * pressed_keys[6] being the last key pressed
     */
    uint16_t pressed_keys[7];
} kevent_kb_ctx_t;

static inline kevent_kb_ctx_t* kevent_ctx_to_kb(kevent_ctx_t* ctx)
{
    if (ctx->buffer_size != sizeof(kevent_kb_ctx_t))
        return nullptr;

    return ctx->buffer;
}

static inline bool kevent_is_keycombination_pressed(kevent_kb_ctx_t* ctx, enum ANIVA_SCANCODES* keys, uint32_t len)
{
    /* This is a key release =/ */
    if (!ctx->pressed)
        return false;

    for (uint32_t i = 0; i < arrlen(ctx->pressed_keys) && len; i++) {
        /* Mismatch, return false */
        if (ctx->pressed_keys[i] != keys[i])
            return false;

        len--;
    }

    return true;
}

/*
 * Keybuffer struct to quickly store a stream of key events
 *
 * Simple utility to avoid having to write this simple thing again and again
 * NOTE: this struct has no ownership over its buffer. Any user of this utility
 * should handle the buffer memory lifetime themselves
 */
struct kevent_kb_keybuffer {
    uint32_t capacity;
    uint32_t w_idx;
    kevent_kb_ctx_t* buffer;
};

int init_kevent_kb_keybuffer(struct kevent_kb_keybuffer* out, kevent_kb_ctx_t* buffer, uint32_t capacity);

int keybuffer_clear(struct kevent_kb_keybuffer* buffer);

kevent_kb_ctx_t* keybuffer_read_key(struct kevent_kb_keybuffer* buffer, uint32_t* p_r_idx);
int keybuffer_write_key(struct kevent_kb_keybuffer* buffer, kevent_kb_ctx_t* event);

#endif // !__ANIVA_KEVENT_KEYBOARD_TYPE__

#ifndef __ANIVA_KEVENT_KEYBOARD_TYPE__
#define __ANIVA_KEVENT_KEYBOARD_TYPE__
#include "kevent/event.h"
#include <libk/stddef.h>

typedef struct kevent_kb_ctx {
  /* Is the current key pressed or released? */
  uint8_t pressed:1;
  /* Keycode that was typed */
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

#endif // !__ANIVA_KEVENT_KEYBOARD_TYPE__

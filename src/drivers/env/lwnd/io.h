#ifndef __ANIVA_LWND_IO__
#define __ANIVA_LWND_IO__

#include "libk/data/bitmap.h"
#include "oss/obj.h"
#include <libk/stddef.h>

#define LWND_MOUSE_LEFT_BTN     0x0001
#define LWND_MOUSE_RIGHT_BTN    0x0002
#define LWND_MOUSE_MIDDLE_BTN   0x0004
#define LWND_MOUSE_BTN_4        0x0008
#define LWND_MOUSE_BTN_5        0x0010

enum LWND_SPRITE_TYPE {
  LWND_SPRITE_TYPE_BMP = 0,
  LWND_SPRITE_TYPE_BITMAP,
  LWND_SPRITE_TYPE_JPG,
  LWND_SPRITE_TYPE_PNG,
};

#define LWND_MAX_MOUSE_SIZE 64

/*
 * The mouse struture for lwnd
 *
 * This structure keeps track of the resources used to display the mouse
 */
typedef struct lwnd_mouse {
  uint32_t x, y;
  /* Both width and height */
  uint16_t size;
  uint16_t btn_flags;

  enum LWND_SPRITE_TYPE sprite_type;

  union {
    oss_obj_t* file;
    bitmap_t* bitmap;
  } sprite;
} lwnd_mouse_t;

void init_lwnd_mouse(uint32_t start_x, uint32_t start_y, uint16_t size, enum LWND_SPRITE_TYPE type, void* sprite);

int lwnd_draw_mouse_cursor(lwnd_mouse_t* mouse);

/*
 * The keyboard of lwnd
 *
 * This keeps track of the currently pressed key either in the form of 
 * UTF-8 or UTF-16
 *
 * This also needs a HID driver as its source
 */
typedef struct lwnd_keyboard {
  union {
    uint16_t utf16;
    uint8_t utf8;
  } format;

  bool is_utf16 : 1;

  void* hid_source;

} lwnd_keyboard_t;

void init_lwnd_keyboard();

#endif // !__ANIVA_LWND_IO__

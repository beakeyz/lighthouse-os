#ifndef __ANIVA_HID_EVENTS__
#define __ANIVA_HID_EVENTS__

#include "hid.h"
#include "lightos/event/key.h"

enum HID_EVENT_TYPE {
 HID_EVENT_KEYPRESS = 0,
 HID_EVENT_MOUSE_MOVE,
 HID_EVENT_STATUS_CHANGE,
 HID_EVENT_CONNECT,
 HID_EVENT_DISCONNECT,
};

#define HID_EVENTNAME "hid"

#define HID_EVENT_KEY_FLAG_PRESSED  0x0001
#define HID_EVENT_KEY_FLAG_RESET    0x0002

#define HID_MOUSE_FLAG_LBTN_PRESSED 0x0001
#define HID_MOUSE_FLAG_RBTN_PRESSED 0x0002
#define HID_MOUSE_FLAG_MBTN_PRESSED 0x0004


/*
 * Event that a HID device can generate
 */
typedef struct hid_event {
  enum HID_EVENT_TYPE type;

  union {
    /* Keyboard event */
    struct {
      /* Keycode that was typed */
      uint32_t keycode;
      /* Can be null. Depends on the underlying driver */
      uint8_t pressed_char;
      /* Is the current key pressed or released? */
      uint8_t pressed:1;
      /*
       * All the keys that are currently pressed 
       * with pressed_keys[0] being the first and
       * pressed_keys[6] being the last key pressed
       */
      uint16_t pressed_keys[7];
    } key;
    /* Mouse event */
    struct {
      uint32_t deltax;
      uint32_t deltay;
      uint32_t deltaz;
      uint16_t flags;
    } mouse;
  };

  struct hid_device* device;
} hid_event_t;

hid_event_t* create_hid_event(struct hid_device* dev, enum HID_EVENT_TYPE type);
void destroy_hid_event(hid_event_t* event);

bool hid_event_is_keycombination_pressed(hid_event_t* ctx, enum ANIVA_SCANCODES* keys, uint32_t len);

/*
 * Keybuffer struct to quickly store a stream of hid key events
 *
 * Simple utility to avoid having to write this simple thing again and again
 * NOTE: this struct has no ownership over its buffer. Any user of this utility 
 * should handle the buffer memory lifetime themselves
 */
struct hid_event_keybuffer {
  size_t capacity;
  uint32_t w_idx;
  uint32_t r_idx;
  hid_event_t* buffer;
};

int init_hid_event_keybuffer(struct hid_event_keybuffer* out, hid_event_t* buffer, uint32_t capacity);
int keybuffer_clear(struct hid_event_keybuffer* buffer);
hid_event_t* keybuffer_read_key(struct hid_event_keybuffer* buffer);
int keybuffer_write_key(struct hid_event_keybuffer* buffer, hid_event_t* event);

/*
 * Code that manages HID events. Since there can be a lot of hid event fires, we'll queue them up
 * and let them be collected on demand.
 */

void init_hid_events();
kerror_t queue_hid_event(hid_event_t* event);

#endif // !__ANIVA_HID_EVENTS__

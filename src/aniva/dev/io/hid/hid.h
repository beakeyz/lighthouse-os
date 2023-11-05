#ifndef __ANIVA_HID_CORE__
#define __ANIVA_HID_CORE__

#include <libk/stddef.h>
#include <dev/device.h>

struct hid_device;

enum HID_EVENT_TYPE {
 HID_EVENT_KEYPRESS = 0,
 HID_EVENT_MOUSE_MOVE,
 HID_EVENT_STATUS_CHANGE,
 HID_EVENT_CONNECT,
 HID_EVENT_DISCONNECT,
};

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
      uint16_t scancode;
      uint16_t flags;
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

  /* Link events for a singular device */
  struct hid_event* next;
} hid_event_t;

enum HID_BUS_TYPE {
  HID_BUS_TYPE_PS2 = 0,
  HID_BUS_TYPE_USB,
};

typedef struct hid_device {
  /* TODO: how will HID devices look? */

  /* Generic device */
  device_t* dev;
  enum HID_BUS_TYPE btype;

  /*
   * This is where we link any events that this device generates 
   * TODO: Should this be a ringbuffer
   */
  hid_event_t* device_events;
} hid_device_t;

#define HID_DCC_REGISTER_KB_LISTENER 20
#define HID_DCC_UNREGISTER_KB_LISTENER 21
#define HID_DCC_REGISTER_MOUSE_LISTENER 22
#define HID_DCC_UNREGISTER_MOUSE_LISTENER 23

/* Multiplier for mouse change */
#define HID_DCC_SET_MOUSE_SENS 24
/* How rapid are key repeats, when a key is kept pressed? */
#define HID_DCC_SET_KEY_SENS 24

void init_hid();

#endif // !__ANIVA_HID_CORE__

#ifndef __ANIVA_HID_CORE__
#define __ANIVA_HID_CORE__

#include <libk/stddef.h>

/*
 * The Aniva HID subsystem
 */

struct device;
struct hid_event;
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
 * Opperations that most if not all HID devices
 * will be able to implement
 */
struct device_hid_endpoint {
  /* Check if there is a HID event from this device */
  int (*f_poll)(struct device* dev, struct hid_event* event);
};


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
  struct device* dev;
  enum HID_BUS_TYPE btype;

  /*
   * This is where we link any events that this device generates 
   * TODO: Should this be a ringbuffer
   */
  hid_event_t* device_events;
} hid_device_t;

void init_hid();

#endif // !__ANIVA_HID_CORE__

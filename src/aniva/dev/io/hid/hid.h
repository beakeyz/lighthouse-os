#ifndef __ANIVA_HID_CORE__
#define __ANIVA_HID_CORE__

#include "libk/flow/error.h"
#include <libk/stddef.h>

/*
 * The Aniva HID subsystem
 */

struct device;
struct hid_event;
struct hid_device;
struct device_endpoint;

/*
 * Opperations that most if not all HID devices
 * will be able to implement
 */
struct device_hid_endpoint {
  /* Check if there is a HID event from this device */
  int (*f_poll)(struct device* dev, struct hid_event* event);
};

enum HID_BUS_TYPE {
  HID_BUS_TYPE_PS2 = 0,
  HID_BUS_TYPE_USB,
};

typedef struct hid_device {
  /* TODO: how will HID devices look? */

  /* Generic device */
  struct device* dev;
  enum HID_BUS_TYPE btype;
} hid_device_t;

void init_hid();

hid_device_t* create_hid_device(const char* name, enum HID_BUS_TYPE btype, struct device_endpoint* eps);
void destroy_hid_device(hid_device_t* device);

kerror_t register_hid_device(hid_device_t* device);
kerror_t unregister_hid_device(hid_device_t* device);
hid_device_t* get_hid_device(const char* name);

#endif // !__ANIVA_HID_CORE__

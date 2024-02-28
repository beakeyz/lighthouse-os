#ifndef __ANIVA_USB_PORT__
#define __ANIVA_USB_PORT__

#include "dev/usb/usb.h"

typedef struct usb_port {
  enum USB_SPEED speed;
} usb_port_t;

#endif // !__ANIVA_USB_PORT__

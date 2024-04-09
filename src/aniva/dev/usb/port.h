#ifndef __ANIVA_USB_PORT__
#define __ANIVA_USB_PORT__

#include "dev/usb/spec.h"

typedef struct usb_port {
  usb_port_status_t status;
} usb_port_t;

#endif // !__ANIVA_USB_PORT__

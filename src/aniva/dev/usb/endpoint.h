#ifndef __ANIVA_USB_ENDPOINT__
#define __ANIVA_USB_ENDPOINT__

#include <libk/stddef.h>

struct usb_device;

/*
 * A particular endpoint on a USB device
 */
typedef struct usb_endpoint {
  uint32_t ep;
  struct usb_device* device;
} usb_endpoint_t;

#endif // !__ANIVA_USB_ENDPOINT__

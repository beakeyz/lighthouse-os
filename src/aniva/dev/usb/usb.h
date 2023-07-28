#ifndef __ANIVA_USB_DEF__
#define __ANIVA_USB_DEF__

typedef struct usb_device {
  int id;
  char* product;
  char* manufacturer;
  char* serial;
} usb_device_t;

#endif // !__ANIVA_USB_DEF__

#ifndef __ANIVA_USB_PORT__
#define __ANIVA_USB_PORT__

enum USB_PORT_PROTOCOL {
  USB_PROT_USB1,
  USB_PROT_USB2,
  USB_PROT_USB3
};

typedef struct usb_port {
  enum USB_PORT_PROTOCOL protocol;
} usb_port_t;

#endif // !__ANIVA_USB_PORT__

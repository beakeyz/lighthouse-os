#ifndef __ANIVA_USB_PORT__
#define __ANIVA_USB_PORT__

#include "dev/usb/spec.h"

struct usb_device;

typedef struct usb_port {
    usb_port_status_t status;

    /* Private HCD specific shit */
    void* priv;
    /* NOTE: Memory not owned by struct usb_port */
    struct usb_device* device;
} usb_port_t;

static inline bool usb_port_has_connchange(usb_port_t* port)
{
    return ((port->status.change & USB_PORT_STATUS_CONNECTION) == USB_PORT_STATUS_CONNECTION);
}

static inline bool usb_port_is_connected(usb_port_t* port)
{
    return ((port->status.status & USB_PORT_STATUS_CONNECTION) == USB_PORT_STATUS_CONNECTION);
}

static inline bool usb_port_is_uninitialised(usb_port_t* port)
{
    return (usb_port_is_connected(port) && !port->device);
}

#endif // !__ANIVA_USB_PORT__

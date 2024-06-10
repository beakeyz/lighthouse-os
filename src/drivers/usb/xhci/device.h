#ifndef __ANIVA_DEV_USB_XHCI_DEVICE__
#define __ANIVA_DEV_USB_XHCI_DEVICE__

#include "dev/usb/usb.h"
#include "drivers/usb/xhci/xhci.h"
#include <libk/stddef.h>

/*
 * Device on a xHC HCD
 */
typedef struct xhci_device {
    uint8_t slot, addr;

    xhci_ring_t* ep_rings;
    usb_device_t* udev;
} xhci_device_t;

xhci_device_t* create_xhci_device();

#endif // !__ANIVA_DEV_USB_XHCI_DEVICE__

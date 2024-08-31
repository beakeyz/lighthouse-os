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

    /* The rings for the endpoints on this device */
    xhci_ring_t* ep_rings;

    /* This devices context */
    paddr_t device_ctx_dma;
    xhci_device_ctx_t* device_ctx;

    /* This devices input context */
    paddr_t input_device_ctx_dma;
    xhci_input_device_ctx_t* input_ctx;

    usb_device_t* udev;
} xhci_device_t;

xhci_device_t* create_xhci_device();

#endif // !__ANIVA_DEV_USB_XHCI_DEVICE__

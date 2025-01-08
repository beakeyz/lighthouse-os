#ifndef __ANIVA_USB_HCD__
#define __ANIVA_USB_HCD__

#include "dev/device.h"
#include "dev/pci/pci.h"
#include "libk/flow/reference.h"
#include <libk/stddef.h>

struct device;
struct usb_hcd;
struct usb_hub;
struct usb_xfer;
struct driver;

typedef struct usb_hcd_io_ops {
    int (*enq_request)(struct usb_hcd* hcd, struct usb_xfer* request);
    int (*deq_request)(struct usb_hcd* hcd, struct usb_xfer* request);
} usb_hcd_io_ops_t;

typedef struct usb_hcd_hw_ops {
    int (*hcd_setup)(struct usb_hcd* hcd);
    int (*hcd_start)(struct usb_hcd* hcd);
    void (*hcd_stop)(struct usb_hcd* hcd);
    void (*hcd_cleanup)(struct usb_hcd* hcd);
} usb_hcd_hw_ops_t;

/*
 * One host controller driver per controller
 */
typedef struct usb_hcd {
    flat_refc_t ref;
    u32 id;

    void* private;

    /* TODO: is it a given that usb hubs are on the PCI bus? */
    pci_device_t* pci_device;
    struct usb_hub* roothub;
    struct driver* driver;

    usb_hcd_io_ops_t* io_ops;
    usb_hcd_hw_ops_t* hw_ops;

    /* Bitmap to manage device address allocation */
    bitmap_t* devaddr_bitmap;

    /* Locks access to the hcds fields */
    mutex_t* hcd_lock;
} usb_hcd_t;

static inline struct device* usb_hcd_get_device(usb_hcd_t* hcd)
{
    return hcd->pci_device->dev;
}

usb_hcd_t* create_usb_hcd(struct driver* driver, pci_device_t* host, char* hub_name, device_ctl_node_t* ctllist);
void destroy_usb_hcd(usb_hcd_t* hub);

int usb_hcd_alloc_devaddr(usb_hcd_t* hub, uint8_t* paddr);
int usb_hcd_dealloc_devaddr(usb_hcd_t* hub, uint8_t addr);

bool hcd_is_accessed(struct usb_hcd* hcd);

#endif // !__ANIVA_USB_HCD__

#ifndef __ANIVA_USB_DEF__
#define __ANIVA_USB_DEF__

/*
 * USB subsystem
 *
 * most USB devices will be connected through PCI so the different types of hosts
 * will get initialized through PCI drivers. The hosts will in turn initialize their own
 * USB device specific drivers, based on the attached devices. For example, a USB mass
 * storage device will probably just register another generic disk device object, register it and
 * then implement the generic high-level mass storage functions. Same goes for HID devices, like keyboards
 * and mice. These devices get a backend loaded on the PCI USB, but their frontends will simply consist of the hid_device
 * that gets created and registered
 */

#include "dev/pci/pci.h"
#include "dev/usb/spec.h"
#include "dev/usb/xfer.h"
#include "libk/flow/doorbell.h"
#include <libk/stddef.h>

struct usb_hcd;
struct usb_hub;
struct device;
struct usb_device;
struct usb_xfer;

enum USB_SPEED {
    USB_LOWSPEED,
    USB_FULLSPEED,
    USB_HIGHSPEED,
    USB_SUPERSPEED
};

void init_usb();
void load_usb_hcds();

int enumerate_usb_devices(void (*f_cb)(struct usb_device*, void*), void* param);

/*
 * Generic USB device structure
 *
 * Holds general information about how we can communicate with a particular device
 * and what its capabilities are
 *
 * A usb device lives on a certain port / slot and thus has an address
 * on the hub we need to keep track of.
 *
 * It also has (at least one) endpoint(s) that we also need to track here, so we
 * can redirect requests/transfers to/from the correct endpoints
 */
typedef struct usb_device {
    /* When we discover a device, this is the first descriptor we'll get back */
    usb_device_descriptor_t desc;

    uint8_t hub_port; /* Port of the devices hub */
    uint8_t hub_addr;
    uint8_t dev_port; /* From 0 to 255, what is our index? (nth device found on the hub) */
    uint8_t dev_addr;
    uint8_t slot;
    uint8_t ep_count;
    uint8_t config_count;
    uint8_t active_config;

    char* product;
    char* manufacturer;
    char* serial;

    uint32_t state;
    enum USB_SPEED speed;

    /* List of configurations for this device */
    usb_config_buffer_t** configuration_arr;

    /*
     * Doorbell that rings on request completion and
     * throws the response into a buffer
     */
    kdoorbell_t* req_doorbell;

    /* Parent hub this device is located on */
    struct device* device;
    struct usb_hcd* hcd;
    struct usb_hub* hub;
} usb_device_t;

static inline bool usb_device_is_hub(usb_device_t* device)
{
    return device->desc.dev_class == 0x9;
}

usb_device_t* create_usb_device(struct usb_hcd* hcd, struct usb_hub* hub, enum USB_SPEED speed, uint8_t dev_port, uint8_t hub_port, const char* name);
void destroy_usb_device(usb_device_t* device);
struct usb_hcd* usb_device_get_hcd(usb_device_t* device);

int usb_device_get_class_descriptor(usb_device_t* device, uint8_t descriptor_type, uint8_t index, uint16_t language_id, void* buffer, size_t bsize);
int usb_device_get_descriptor(usb_device_t* device, uint8_t descriptor_type, uint8_t index, uint16_t language_id, void* buffer, size_t bsize);
int usb_device_set_address(usb_device_t* device, uint8_t addr);
int usb_device_set_config(usb_device_t* device, uint8_t config_idx);
int usb_device_get_active_config(usb_device_t* device, usb_config_buffer_t* conf_buf);
int usb_device_reset_address(usb_device_t* device);

int usb_device_submit_ctl(usb_device_t* device, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len);
int usb_device_submit_int(usb_device_t* device, usb_xfer_t** pxfer, int (*f_cb)(struct usb_xfer*), enum USB_XFER_DIRECTION direct, usb_endpoint_buffer_t* epb, void* buffer, size_t bsize);

/*
 * TODO: Define the functions for handling transfers to specific endpoints on a device
 */

enum USB_HUB_TYPE {
    USB_HUB_TYPE_MOCK, /* Mock hubs are intermediate hubs that act as bridges */
    USB_HUB_TYPE_EHCI,
    USB_HUB_TYPE_OHCI,
    USB_HUB_TYPE_UHCI,
    USB_HUB_TYPE_XHCI,
};

#define USB_HC_PORT 0
/* This is the port of our roothub */
#define USB_ROOTHUB_PORT 1

#define USBHUB_FLAGS_ENUMERATING 0x0001
#define USBHUB_FLAGS_ACTIVE 0x0002

/*
 * Every hub is a usb device but not every
 * usb device is a hub. Get it?
 */
typedef struct usb_hub {
    struct usb_device* udev;
    struct dgroup* devgroup;
    struct usb_hub_descriptor hubdesc;
    uint16_t flags;

    enum USB_HUB_TYPE type;

    uint32_t portcount;
    struct usb_port* ports;

    struct usb_hub* parent;
    struct usb_hcd* hcd;
} usb_hub_t;

int create_usb_hub(usb_hub_t** phub, struct usb_hcd* hcd, enum USB_HUB_TYPE type, usb_hub_t* parent, usb_device_t* device, uint32_t portcount, bool do_init);
int init_usb_device(usb_device_t* device);
void destroy_usb_hub(usb_hub_t* hub);

int usb_hub_handle_port_connection(usb_hub_t* hub, uint32_t i);

int usb_device_submit_hc_ctl(usb_device_t* dev, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len);
int usb_hub_enumerate(usb_hub_t* hub);

/*
 * HCD stuff
 */

struct usb_hcd* alloc_usb_hcd();
void dealloc_usb_hcd(struct usb_hcd* hcd);

/* Refcounting */
struct usb_hcd* get_hcd_for_pci_device(pci_device_t* device);
void release_usb_hcd(struct usb_hcd* hcd);

int register_usb_hcd(struct usb_hcd* hub);
int unregister_usb_hcd(struct usb_hcd* hub);

/* Transfer stuff */
struct usb_xfer* allocate_usb_xfer();
void deallocate_usb_xfer(struct usb_xfer* req);

#endif // !__ANIVA_USB_DEF__

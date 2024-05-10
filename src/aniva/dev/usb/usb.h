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
#include "libk/flow/doorbell.h"
#include <libk/stddef.h>

struct usb_hcd;
struct usb_hub;
struct device;
struct usb_xfer;

enum USB_SPEED {
  USB_LOWSPEED,
  USB_FULLSPEED,
  USB_HIGHSPEED,
  USB_SUPERSPEED
};

void init_usb();
void init_usb_drivers();

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

  uint8_t hub_port; /* From 0 to 255, what is our index? (nth device found on the hub) */
  uint8_t dev_addr;
  uint8_t slot;
  uint8_t ep_count;

  char* product;
  char* manufacturer;
  char* serial;

  uint32_t state;
  enum USB_SPEED speed;

  /*
   * Doorbell that rings on request completion and
   * throws the response into a buffer
   */
  kdoorbell_t* req_doorbell;

  /* Parent hub this device is located on */
  struct device* device;
  struct usb_hub* hub;
} usb_device_t;

static inline bool usb_device_is_hub(usb_device_t* device)
{
  return device->desc.dev_class == 0x9;
}

usb_device_t* create_usb_device(struct usb_hcd* hcd, struct usb_hub* hub, enum USB_SPEED speed, uint8_t hub_port, const char* name);
void destroy_usb_device(usb_device_t* device);
struct usb_hcd* usb_device_get_hcd(usb_device_t* device);

int usb_device_submit_ctl(usb_device_t* device, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len);

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

/*
 * Every hub is a usb device but not every
 * usb device is a hub. Get it?
 */
typedef struct usb_hub {
  struct usb_device* device;
  struct dgroup* devgroup;
  struct usb_hub_descriptor hubdesc;

  enum USB_HUB_TYPE type;

  uint32_t portcount;
  struct usb_port* ports;
  
  struct usb_hub* parent;
  struct usb_hcd* hcd;
} usb_hub_t;

usb_hub_t* create_usb_hub(struct usb_hcd* hcd, enum USB_HUB_TYPE type, usb_hub_t* parent, uint8_t hubidx, uint8_t d_addr, uint32_t portcount);
int init_usb_device(usb_device_t* device);
void destroy_usb_hub(usb_hub_t* hub);

int usb_hub_submit_default_ctl(usb_hub_t* hub, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len);
int usb_hub_submit_ctl(usb_hub_t* hub, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len);
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

/*
 * TODO: usb drivers
 *
 * These things can get registerd by class drivers which want to identify usb devices which they know
 * how to communicate with
 */
typedef struct usb_driver {
  int (*f_probe)(struct usb_driver* driver, usb_device_t* device);

  uint32_t flags;
} usb_driver_t;

#endif // !__ANIVA_USB_DEF__

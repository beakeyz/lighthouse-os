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
#include "libk/data/linkedlist.h"
#include "libk/flow/doorbell.h"
#include "sync/mutex.h"
#include "system/resource.h"
#include <libk/stddef.h>

#define USB_DEVICE_TYPE_HID 0x00
#define USB_DEVICE_TYPE_MASS_STORAGE 0x01
#define USB_DEVICE_TYPE_MISC 0x02

struct usb_hcd;
struct usb_hub;
struct usb_request;

typedef struct usb_device {
  usb_device_descriptor_t desc;
  uint8_t port_num; /* From 0 to 255, which port are we on? */
  uint8_t effective_idx; /* From 0 to 255, what is our index? (nth device found on the hub) */
  uint8_t dev_type;

  char* product;
  char* manufacturer;
  char* serial;

  uint32_t state;
  uint32_t speed;

  kdoorbell_t* req_doorbell;

  struct usb_hub* hub;
} usb_device_t;

usb_device_t* create_usb_device(struct usb_hub* hub, uint8_t port_num);
void destroy_usb_device(usb_device_t* device);

struct usb_hcd* usb_device_get_hcd(usb_device_t* device);

#define USB_HUB_TYPE_MOCK 0x00 /* Mock hubs are intermediate hubs that act as bridges */
#define USB_HUB_TYPE_EHCI 0x01
#define USB_HUB_TYPE_OHCI 0x02
#define USB_HUB_TYPE_UHCI 0x03
#define USB_HUB_TYPE_XHCI 0x04

#define USB_HUB_TYPE_MAX (USB_HUB_TYPE_XHCI)

/*
 * Every hub is a usb device but not every
 * usb device is a hub. Get it?
 */
typedef struct usb_hub {
  struct usb_device* device;

  uint8_t dev_addr;
  
  struct usb_hub* parent;
  struct usb_hcd* hcd;
} usb_hub_t;

usb_hub_t* create_usb_hub(struct usb_hcd* hcd, usb_hub_t* parent, uint8_t d_addr, uint8_t p_num);
void destroy_usb_hub(usb_hub_t* hub);

/*
 * This represents a data transfer (request) over USB
 */
typedef struct usb_transfer {
  void* tx_buffer;
  uint32_t tx_buffer_len;

  struct usb_device* device;
} usb_transfer_t;

void init_usb();

struct usb_hcd* alloc_usb_hcd();
void dealloc_usb_hcd(struct usb_hcd* hcd);

#define MAX_USB_HCDS 256

/* Refcounting */
struct usb_hcd* get_usb_hcd(uint8_t index);
struct usb_hcd* get_hcd_for_pci_device(pci_device_t* device);
void release_usb_hcd(struct usb_hcd* hcd);

int register_usb_hcd(struct usb_hcd* hub);
int unregister_usb_hcd(struct usb_hcd* hub);

static inline bool is_valid_usb_hub_type(uint8_t type)
{
  return (type <= USB_HUB_TYPE_MAX);
}

struct usb_request* allocate_usb_request();
void deallocate_usb_request(struct usb_request* req);

#endif // !__ANIVA_USB_DEF__

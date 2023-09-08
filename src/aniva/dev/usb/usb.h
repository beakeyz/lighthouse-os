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
#include "libk/data/linkedlist.h"
#include "sync/mutex.h"
#include "system/resource.h"
#include <libk/stddef.h>

#define USB_DEVICE_TYPE_HID 0x00
#define USB_DEVICE_TYPE_MASS_STORAGE 0x01
#define USB_DEVICE_TYPE_MISC 0x02

struct usb_hcd;

typedef struct usb_device {
  /* TODO: how will generic USB devices look? */
  uint8_t port_num; /* From 0 to 255, which port are we on? */
  uint8_t effective_idx; /* From 0 to 255, what is our index? (nth device found on the hub) */
  uint8_t dev_type;

  char* product;
  char* manufacturer;
  char* serial;

  struct usb_hcd* hcd;
} usb_device_t;

#define USB_HUB_TYPE_MOCK 0x00 /* Mock hubs are intermediate hubs that act as bridges */
#define USB_HUB_TYPE_EHCI 0x01
#define USB_HUB_TYPE_OHCI 0x02
#define USB_HUB_TYPE_UHCI 0x03
#define USB_HUB_TYPE_XHCI 0x04

#define USB_HUB_TYPE_MAX (USB_HUB_TYPE_XHCI)

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
void release_usb_hcd(struct usb_hcd* hcd);

int register_usb_hcd(struct usb_hcd* hub);
int unregister_usb_hcd(struct usb_hcd* hub);

static inline bool is_valid_usb_hub_type(uint8_t type)
{
  return (type <= USB_HUB_TYPE_MAX);
}


#endif // !__ANIVA_USB_DEF__

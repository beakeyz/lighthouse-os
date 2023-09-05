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

struct usb_hub;

typedef struct usb_device {
  /* TODO: how will generic USB devices look? */
  uint8_t port_num; /* From 0 to 255, which port are we on? */
  uint8_t effective_idx; /* From 0 to 255, what is our index? (nth device found on the hub) */
  uint8_t dev_type;

  char* product;
  char* manufacturer;
  char* serial;

  struct usb_hub* hub;
} usb_device_t;

#define USB_HUB_TYPE_MOCK 0x00 /* Mock hubs are intermediate hubs that act as bridges */
#define USB_HUB_TYPE_EHCI 0x01
#define USB_HUB_TYPE_OHCI 0x02
#define USB_HUB_TYPE_UHCI 0x03
#define USB_HUB_TYPE_XHCI 0x04

#define USB_HUB_TYPE_MAX (USB_HUB_TYPE_XHCI)

typedef struct usb_hub_mmio_ops {
  uint32_t (*mmio_read32)(struct usb_hub* hub, uintptr_t offset);
  void (*mmio_write32)(struct usb_hub* hub, uintptr_t offset, uint32_t value);

  uint16_t (*mmio_read16)(struct usb_hub* hub, uintptr_t offset);
  void (*mmio_write16)(struct usb_hub* hub, uintptr_t offset, uint16_t value);

  uint8_t (*mmio_read8)(struct usb_hub* hub, uintptr_t offset);
  void (*mmio_write8)(struct usb_hub* hub, uintptr_t offset, uint8_t value);

  /* TODO: */
} usb_hub_mmio_ops_t;

typedef struct usb_hub {
  uint8_t hub_type;

  /* Name of the hub: can be a custom name */
  char* hub_name;

  void* private;

  /* TODO: is it a given that usb hubs are on the PCI bus? */
  pci_device_t* host_device;
  usb_hub_mmio_ops_t* mmio_ops;

  struct usb_hub* parent;

  /*
   * Since a single usb hub is only going to allocate it's registers and some
   * random buffers, we can use a resource list here =)
   */
  resource_list_t* resources;

  list_t* child_hubs;
  list_t* devices;
} usb_hub_t;

void init_usb();

usb_hub_t* create_usb_hub(pci_device_t* host, char* hub_name, uint8_t type);
void destroy_usb_hub(usb_hub_t* hub);

int register_usb_hub(usb_hub_t* hub);
int unregister_usb_hub(usb_hub_t* hub);

#endif // !__ANIVA_USB_DEF__

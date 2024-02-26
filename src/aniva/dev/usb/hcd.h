#ifndef __ANIVA_USB_HCD__
#define __ANIVA_USB_HCD__

#include "dev/device.h"
#include "dev/pci/pci.h"
#include "libk/flow/reference.h"
#include "system/resource/list.h"
#include <libk/stddef.h>

struct usb_hcd;
struct usb_request;

/*
 * MMIO operations for hcds
 */
typedef struct usb_hcd_mmio_ops {
  uint64_t (*mmio_read64)(struct usb_hcd* hub, uintptr_t offset);
  void (*mmio_write64)(struct usb_hcd* hub, uintptr_t offset, uint64_t value);

  uint32_t (*mmio_read32)(struct usb_hcd* hub, uintptr_t offset);
  void (*mmio_write32)(struct usb_hcd* hub, uintptr_t offset, uint32_t value);

  uint16_t (*mmio_read16)(struct usb_hcd* hub, uintptr_t offset);
  void (*mmio_write16)(struct usb_hcd* hub, uintptr_t offset, uint16_t value);

  uint8_t (*mmio_read8)(struct usb_hcd* hub, uintptr_t offset);
  void (*mmio_write8)(struct usb_hcd* hub, uintptr_t offset, uint8_t value);

  int (*mmio_wait_read)(struct usb_hcd* hub, uintptr_t max_timeout, void* address, uintptr_t mask, uintptr_t expect);
} usb_hcd_mmio_ops_t;

typedef struct usb_hcd_io_ops {
  int (*enq_request)(struct usb_hcd* hcd, uint8_t usb_req, void* buffer, size_t bsize);
} usb_hcd_io_ops_t;

typedef struct usb_hcd_hw_ops {
  int (*hcd_setup) (struct usb_hcd* hcd);
  int (*hcd_start) (struct usb_hcd* hcd);
  void (*hcd_stop) (struct usb_hcd* hcd);
} usb_hcd_hw_ops_t;

/*
 * One host controller driver per controller
 */
typedef struct usb_hcd {
  uint8_t hub_type;

  flat_refc_t ref;

  /* Name of the hub: can be a custom name */
  char* hub_name;

  void* private;

  /* TODO: is it a given that usb hubs are on the PCI bus? */
  pci_device_t* pci_device;

  usb_hcd_mmio_ops_t* mmio_ops;
  usb_hcd_io_ops_t* io_ops;
  usb_hcd_hw_ops_t* hw_ops;

  /* Can be null */
  struct usb_hcd* parent;

  /*
   * Since a single usb hub is only going to allocate it's registers and some
   * random buffers, we can use a resource list here =)
   */
  resource_list_t* resources;

  /* Locks access to the hcds fields */
  mutex_t* hcd_lock;

  /* FIXME: replace with something a lil more better =) */
  list_t* child_hubs;
  list_t* devices;
} usb_hcd_t;

usb_hcd_t* create_usb_hcd(pci_device_t* host, char* hub_name, uint8_t type, struct device_endpoint *eps, uint32_t ep_count);
void destroy_usb_hcd(usb_hcd_t* hub);

bool hcd_is_accessed(struct usb_hcd* hcd);

#endif // !__ANIVA_USB_HCD__

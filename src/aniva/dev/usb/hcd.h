#ifndef __ANIVA_USB_HCD__
#define __ANIVA_USB_HCD__

#include "dev/pci/pci.h"
#include "libk/flow/reference.h"
#include <libk/stddef.h>

struct device;
struct usb_hcd;
struct usb_hub;
struct usb_xfer;
struct drv_manifest;
struct device_endpoint;

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
  int (*enq_request)(struct usb_hcd* hcd, struct usb_xfer* request);
  int (*deq_request)(struct usb_hcd* hcd, struct usb_xfer* request);
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
  flat_refc_t ref;

  void* private;

  /* TODO: is it a given that usb hubs are on the PCI bus? */
  pci_device_t* pci_device;
  struct usb_hub* roothub;
  struct drv_manifest* driver;

  usb_hcd_mmio_ops_t* mmio_ops;
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

usb_hcd_t* create_usb_hcd(struct drv_manifest* driver, pci_device_t* host, char* hub_name, struct device_endpoint *eps);
void destroy_usb_hcd(usb_hcd_t* hub);

int usb_hcd_alloc_devaddr(usb_hcd_t* hub, uint8_t* paddr);
int usb_hcd_dealloc_devaddr(usb_hcd_t* hub, uint8_t addr);

bool hcd_is_accessed(struct usb_hcd* hcd);

#endif // !__ANIVA_USB_HCD__

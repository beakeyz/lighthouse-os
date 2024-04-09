/* Core USB functions */
#include "usb.h"
#include "dev/device.h"
#include "dev/group.h"
#include "dev/loader.h"
#include "dev/pci/pci.h"
#include "dev/usb/hcd.h"
#include "dev/usb/port.h"
#include "dev/usb/spec.h"
#include "dev/usb/xfer.h"
#include "libk/cmdline/parser.h"
#include "libk/data/bitmap.h"
#include "libk/flow/doorbell.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "mem/heap.h"
#include "mem/zalloc.h"
#include "proc/profile/profile.h"

zone_allocator_t __usb_hub_allocator;
zone_allocator_t __usb_xfer_allocator;
dgroup_t* _root_usbhub_group;

/*!
 * @brief Allocate memory for a HCD
 *
 * Nothing to add here...
 */
usb_hcd_t* alloc_usb_hcd()
{
  usb_hcd_t* ret;

  ret = zalloc_fixed(&__usb_hub_allocator);

  memset(ret, 0, sizeof(usb_hcd_t));

  return ret;
}

/*!
 * @brief Deallocate memory of a HCD
 *
 * Nothing to add here...
 */
void dealloc_usb_hcd(struct usb_hcd* hcd)
{
  if (!hcd)
    return;

  zfree_fixed(&__usb_hub_allocator, hcd);
}

/*!
 * @brief Allocate and initialize a generic USB device
 *
 */
usb_device_t* create_usb_device(struct usb_hub* hub, const char* name)
{
  dgroup_t* group;
  usb_device_t* device;

  device = kmalloc(sizeof(*device));

  if (!device)
    return nullptr;

  memset(device, 0, sizeof(*device));

  device->req_doorbell = create_doorbell(255, NULL);
  device->hub = hub;
  device->device = create_device_ex(NULL, (char*)name, device, NULL, NULL);

  /* Give ourselves a device address */
  usb_hub_alloc_devaddr(hub, &device->dev_addr);

  /* TODO: get the devic descriptor n shit */
  //kernel_panic("TODO: gather USB device info");

  group = _root_usbhub_group;

  if (hub)
    group = hub->devgroup;

  device_register(device->device, group);

  return device;
}

/*!
 * @brief Deallocate a USB device
 *
 */
void destroy_usb_device(usb_device_t* device)
{
  kernel_panic("TODO: destroy_usb_device");
  destroy_doorbell(device->req_doorbell);
  kfree(device);
}


/*!
 * @brief Allocate and initialize a generic USB hub
 *
 * TODO: create our own root hub configuration descriptor
 */
usb_hub_t* create_usb_hub(struct usb_hcd* hcd, usb_hub_t* parent, uint8_t d_addr, uint32_t portcount)
{
  dgroup_t* parent_group;
  usb_hub_t* hub;

  hub = kmalloc(sizeof(*hub));

  if (!hub)
    return nullptr;

  memset(hub, 0, sizeof(*hub));

  parent_group = _root_usbhub_group;

  if (parent)
    parent_group = parent->devgroup;

  hub->parent = parent;
  hub->hcd = hcd;
  hub->portcount = portcount;
  hub->devaddr_bitmap = create_bitmap_ex(128, 0x00);
  /* Register a dev group for this hub */
  hub->devgroup = register_dev_group(DGROUP_TYPE_USB, "???", DGROUP_FLAG_BUS, parent_group->node);
  /* Asks the host controller for a device descriptor */
  hub->device = create_usb_device(hub, "hub");

  if (!hub->device) {
    kfree(hub);
    return nullptr;
  }

  hub->ports = kmalloc(portcount * sizeof(usb_port_t));
  memset(hub->ports, 0, portcount * sizeof(usb_port_t));

  return hub;
}

/*!
 * @brief Deallocate a generic USB hub
 *
 */
void destroy_usb_hub(usb_hub_t* hub)
{
  kernel_panic("TODO: destroy_usb_hub");
}

/*!
 * @brief: Allocate a deviceaddress for a usb device
 */
int usb_hub_alloc_devaddr(usb_hub_t* hub, uint8_t* paddr)
{
  uint64_t addr;
  ErrorOrPtr res;

  res = bitmap_find_free(hub->devaddr_bitmap);

  if (IsError(res))
    return -1;

  addr = Release(res);

  /* Outside of the device address range */
  if (addr >= 128)
    return -1;

  /* Mark as used */
  bitmap_mark(hub->devaddr_bitmap, addr);

  /* Export the address */
  *paddr = (uint8_t)addr;

  return 0;
}

/*!
 * @brief: Deallocate a device address from a hub
 *
 * Called when a device is removed from the hub
 */
int usb_hub_dealloc_devaddr(usb_hub_t* hub, uint8_t addr)
{
  if (!bitmap_isset(hub->devaddr_bitmap, addr))
    return -1;

  bitmap_unmark(hub->devaddr_bitmap, addr);
  return 0;
}

/*!
 * @brief: Submit a control transfer to @hub
 */
int usb_hub_submit_ctl(usb_hub_t* hub, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len)
{
  int error;
  usb_xfer_t* xfer;
  kdoorbell_t* db;
  usb_ctlreq_t ctl;

  db = create_doorbell(1, NULL);
  xfer = create_usb_xfer(NULL, db, NULL, NULL);

  ctl = (usb_ctlreq_t) {
    .request_type = reqtype,
    .request = req,
    .value = value,
    .index = idx,
    .length = len,
  };

  xfer->resp_buffer = respbuf;
  xfer->resp_size = respbuf_len;
  xfer->req_type = USB_CTL_XFER;
  xfer->req_buffer = &ctl;
  xfer->req_size = sizeof(ctl);
  xfer->req_devaddr = 0;
  xfer->req_hubaddr = 0;
  xfer->req_endpoint = 0;

  error = usb_xfer_enqueue(xfer, hub);

  if (error)
    goto dealloc_and_exit;

  usb_await_xfer_complete(xfer, NULL);

dealloc_and_exit:
  destroy_doorbell(db);
  return error;
}

static int usb_hub_get_portsts(usb_hub_t* hub, uint32_t i, usb_port_status_t* status)
{
  return usb_hub_submit_ctl(hub, USB_TYPE_CLASS, USB_REQ_GET_STATUS, 0, i+1, sizeof(usb_port_status_t), status, sizeof(usb_port_status_t));
}

/*!
 * @brief: Check a particular hub for devices
 */
int usb_hub_enumerate(usb_hub_t* hub)
{
  int error;

  for (uint32_t i = 0; i < hub->portcount; i++) {
    printf("%s: Trying to reset port %d\n", hub->device->device->name, i);

    error = usb_hub_get_portsts(hub, i, &hub->ports[i].status);

    if (error)
      continue;

    printf("Got port status:\n\tenabled: %s\n\tconnected: %s\n\tpower: %s\n", 
        (hub->ports[i].status.status & USB_PORT_STATUS_ENABLE) ? "yes" : "no",
        (hub->ports[i].status.status & USB_PORT_STATUS_CONNECTION) ? "yes" : "no",
        (hub->ports[i].status.status & USB_PORT_STATUS_POWER) ? "yes" : "no"
        );
  }

  kernel_panic("Did enumerate");
  return 0;
}

/*!
 * @brief Registers a USB hub directly to the root
 *
 * This only makes the hcd available for searches from the root hcd
 * (which is just a mock bridge) so we know where it is at all times
 */
int register_usb_hcd(usb_hcd_t* hcd)
{
  int error = 0;

  if (!_root_usbhub_group || !hcd->hw_ops || !hcd->hw_ops->hcd_start)
    return -1;

  error = device_register(hcd->pci_device->dev, _root_usbhub_group);

  if (error)
    return error;

  /*
   * NOTE: after setup and stuff, there might be stuff we need to deallocate
   * so FIXME
   */

  /* Sets up memory structures and gathers info so we can propperly use this bus */
  if (hcd->hw_ops->hcd_setup) {
    error = hcd->hw_ops->hcd_setup(hcd);

    if (error)
      goto fail_and_unregister;
  }

  /* Start the controller: Register the roothub and enumerate/configure its devices */
  error = hcd->hw_ops->hcd_start(hcd);

  if (error)
    goto fail_and_unregister;

  return 0;

fail_and_unregister:
  unregister_usb_hcd(hcd);
  return error;
}

/*!
 * @brief Removes a USB hub from the root
 *
 * TODO: more advanced access tracking
 */
int unregister_usb_hcd(usb_hcd_t* hub)
{
  kerror_t error;

  if (!_root_usbhub_group)
    return -1;

  error = device_unregister(hub->pci_device->dev);

  if (error)
    return error;

  return 0;
}

/*!
 * @brief Get the HCD for a specific pci device
 *
 * Nothing to add here...
 */
usb_hcd_t* get_hcd_for_pci_device(pci_device_t* device)
{
  device_t* usb_hcd_device;

  if (!device->dev)
    return nullptr;

  /* If we can find the device in our hcd group, it must contain a HCD struct in it's private field */
  if (dev_group_get_device(_root_usbhub_group, device->dev->name, &usb_hcd_device) || !usb_hcd_device)
    return nullptr;

  return device_unwrap(usb_hcd_device);
}

/*!
 * @brief Release a reference to the hcd
 *
 * For now we only decrement the flat reference counter, but 
 * reference management is a massive subsytem TODO lol
 */
void release_usb_hcd(struct usb_hcd* hcd)
{
  flat_unref(&hcd->ref);

  /*
   * FIXME: do we also destroy the hcd when this hits zero? 
   * FIXME 2: We REALLY need to rethink our reference management model lmao
   */
}


usb_xfer_t* allocate_usb_xfer()
{
  usb_xfer_t* req;

  req = zalloc_fixed(&__usb_xfer_allocator);

  if (!req)
    return nullptr;

  memset(req, 0, sizeof(*req));

  return req;
}

void deallocate_usb_xfer(usb_xfer_t* req)
{
  if (!req)
    return;

  zfree_fixed(&__usb_xfer_allocator, req);
}

/*!
 * @brief: Loads the default usb drivers
 *
 * Called after both the USB subsystem AND the driver subsystem have been initialized
 */
void init_usb_drivers()
{
  if (opt_parser_get_bool(KOPT_NO_USB))
    return;

  /*
   * Load the core driver for the USB subsytem 
   */
  ASSERT_MSG(!!load_external_driver_from_var(USBCORE_DRV_VAR_PATH), "Failed to load USB drivers");
}

/*!
 * @brief Common USB subsytem initialization
 */
void init_usb()
{
  Must(init_zone_allocator(&__usb_hub_allocator, 16 * Kib, sizeof(usb_hcd_t), NULL));
  Must(init_zone_allocator(&__usb_xfer_allocator, 32 * Kib, sizeof(usb_xfer_t), NULL));

  _root_usbhub_group = register_dev_group(DGROUP_TYPE_USB, "usb", NULL, NULL);

  ASSERT_MSG(_root_usbhub_group, "Failed to create vector for hcds");
}

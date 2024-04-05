/* Core USB functions */
#include "usb.h"
#include "dev/device.h"
#include "dev/group.h"
#include "dev/loader.h"
#include "dev/pci/pci.h"
#include "dev/usb/hcd.h"
#include "dev/usb/request.h"
#include "libk/cmdline/parser.h"
#include "libk/flow/doorbell.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "mem/heap.h"
#include "mem/zalloc.h"
#include "proc/profile/profile.h"

zone_allocator_t __usb_hub_allocator;
zone_allocator_t __usb_request_allocator;
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
usb_device_t* create_usb_device(struct usb_hub* hub, const char* name, uint8_t port_num)
{
  dgroup_t* group;
  usb_device_t* device;

  device = kmalloc(sizeof(*device));

  if (!device)
    return nullptr;

  memset(device, 0, sizeof(*device));

  device->req_doorbell = create_doorbell(255, NULL);
  device->port_num = port_num;
  device->hub = hub;
  device->device = create_device_ex(NULL, (char*)name, device, NULL, NULL);

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
usb_hub_t* create_usb_hub(struct usb_hcd* hcd, usb_hub_t* parent, uint8_t d_addr, uint8_t p_num)
{
  dgroup_t* parent_group;
  usb_hub_t* hub;

  hub = kmalloc(sizeof(*hub));

  if (!hub)
    return nullptr;

  parent_group = _root_usbhub_group;

  if (parent)
    parent_group = parent->devgroup;

  hub->parent = parent;
  hub->hcd = hcd;
  /* Asks the host controller for a device descriptor */
  hub->device = create_usb_device(hub, "hub", p_num);

  if (!hub->device) {
    kfree(hub);
    return nullptr;
  }

  /* Register a dev group for this hub */
  hub->devgroup = register_dev_group(DGROUP_TYPE_USB, "???", DGROUP_FLAG_BUS, parent_group->node);

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


usb_request_t* allocate_usb_request()
{
  usb_request_t* req;

  req = zalloc_fixed(&__usb_request_allocator);

  if (!req)
    return nullptr;

  memset(req, 0, sizeof(*req));

  return req;
}

void deallocate_usb_request(usb_request_t* req)
{
  if (!req)
    return;

  zfree_fixed(&__usb_request_allocator, req);
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
  Must(init_zone_allocator(&__usb_request_allocator, 32 * Kib, sizeof(usb_request_t), NULL));

  _root_usbhub_group = register_dev_group(DGROUP_TYPE_USB, "usb", NULL, NULL);

  ASSERT_MSG(_root_usbhub_group, "Failed to create vector for hcds");
}

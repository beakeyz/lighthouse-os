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
#include "libk/flow/doorbell.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "libk/io.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/zalloc.h"
#include "system/profile/profile.h"

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
 * @brief: Send the appropriate control requests to teh HCD to initialize @device
 */
int init_usb_device(usb_device_t* device)
{
  int tries;
  int error;
  usb_hub_t* hub;

  tries = 4;

  do {
    printf("Trying to set device %s addresss to %d\n", device->device->name, device->dev_addr);
    /* Send  */
    error = usb_hub_submit_hc_ctl(device->hub, USB_TYPE_STANDARD | USB_TYPE_DEV_OUT, USB_REQ_SET_ADDRESS, device->dev_addr, 0, NULL, NULL, NULL);

    /* Wait a bit for hardware to catch up */
    mdelay(500);
  } while (tries-- > 0 && error);

  printf("Trying to get device descriptor\n");

  error = usb_device_submit_ctl(device, USB_TYPE_STANDARD | USB_TYPE_DEV_IN, USB_REQ_GET_DESCRIPTOR, USB_DT_DEVICE << 8, 0, 8, &device->desc, 8);
  
  if (error)
    kernel_panic("Failed to get descriptor?");
    // return error;

  printf("Got device descriptor!\n");
  printf("len: %d\n", device->desc.length);
  printf("type: %d\n", device->desc.type);
  printf("usb version: %d\n", device->desc.bcd_usb);
  printf("usb device class: %d\n", device->desc.dev_class);
  printf("usb device subclass: %d\n", device->desc.dev_subclass);
  printf("usb device protocol: %d\n", device->desc.dev_prot);
  printf("usb max packet size: %d\n", device->desc.max_pckt_size);
  
  /* Regular device, yay */
  if (!usb_device_is_hub(device))
    return 0;

  /* Create a new hub -_- */
  if (create_usb_hub(&hub, device->hcd, device->hub->type, device->hub, device, NULL))
    return -1;

  /* 0o0 */
  usb_hub_enumerate(hub);
  return 0;
}

/*!
 * @brief Allocate and initialize a generic USB device
 *
 */
usb_device_t* create_usb_device(struct usb_hcd* hcd, struct usb_hub* hub, enum USB_SPEED speed, uint8_t hub_port, const char* name)
{
  dgroup_t* group;
  usb_device_t* device;

  if (!hcd)
    return nullptr;

  /* Can't have this shit right */
  if (hub && hub_port >= hub->portcount)
    return nullptr;

  device = kmalloc(sizeof(*device));

  if (!device)
    return nullptr;

  memset(device, 0, sizeof(*device));

  device->req_doorbell = create_doorbell(255, NULL);
  device->hub = hub;
  device->hcd = hcd;
  device->speed = speed;
  device->hub_port = hub_port;
  device->device = create_device_ex(NULL, (char*)name, device, NULL, NULL);

  /* 
   * Give ourselves a device address if we are on a hub, otherwise
   * we're at device address zero (we assume we are the roothub device in this case)
   */
  usb_hcd_alloc_devaddr(hcd, &device->dev_addr);

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

static inline int _usb_submit_ctl(usb_hcd_t* hcd, usb_device_t* target, uint8_t devaddr, uint8_t hubaddr, uint8_t hubport, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len)
{
  int error;
  usb_xfer_t* xfer;
  kdoorbell_t* db;
  usb_ctlreq_t ctl;

  if (!target)
    return -1;

  /* Initialize the control transfer */
  init_ctl_xfer(&xfer, &db, &ctl, target, devaddr, hubaddr, hubport,
      reqtype, req, value, idx, len, respbuf, respbuf_len);

  error = usb_xfer_enqueue(xfer, hcd);

  if (error)
    goto dealloc_and_exit;

  (void)usb_await_xfer_complete(xfer, NULL);

  if (xfer->xfer_flags & USB_XFER_FLAG_ERROR)
    error = -KERR_DEV;

dealloc_and_exit:
  release_usb_xfer(xfer);
  destroy_doorbell(db);
  return error;
}

/*!
 * @brief: Submit a control transfer to the control pipe of @device
 *
 * TODO: We should get lower control over transfers
 */
int usb_device_submit_ctl(usb_device_t* device, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len)
{
  uint32_t hubaddr;

  /* Can't send transfers to a device which is not on a hub */
  if (!device)
    return -1;

  /* Default will be the roothub lolol */
  hubaddr = USB_ROOTHUB_PORT;

  if (device->hub)
    hubaddr = device->hub->device->dev_addr;

  return _usb_submit_ctl(device->hcd, device, device->dev_addr, hubaddr, device->hub_port,
      reqtype, req, value, idx, len, respbuf, respbuf_len);
}

int usb_device_get_descriptor(usb_device_t* device, uint8_t descriptor_type, uint8_t index, uint16_t language_id, void* buffer, size_t bsize)
{
  return usb_device_submit_ctl(
    device,
    USB_TYPE_CLASS | USB_TYPE_DEV_IN,
    USB_REQ_GET_DESCRIPTOR,
    (descriptor_type << 8) | index,
    language_id,
    bsize,
    buffer,
    bsize
  );
}

static int _init_usb_hub(usb_hub_t* hub)
{
  int error = usb_device_get_descriptor(
    hub->device,
    USB_DT_HUB,
    0,
    0,
    &hub->hubdesc,
    sizeof(hub->hubdesc)
  );

  if (error)
    return error;

  if (!hub->portcount) {
    hub->portcount = hub->hubdesc.portcount;

    if (!hub->ports)
      hub->ports = kmalloc(hub->portcount * sizeof(usb_port_t));

    /* FUCKKKKKK no memory =( */
    if (!hub->ports)
      return -1;

    memset(hub->ports, 0, hub->portcount * sizeof(usb_port_t));
  }

  for (uint32_t i = 0; i < hub->portcount; i++) {
    if (usb_device_submit_ctl(
      hub->device,
      USB_TYPE_CLASS | USB_TYPE_OTHER_OUT,
      USB_REQ_SET_FEATURE,
      USB_FEATURE_PORT_POWER,
      i+1, 
      0,
      nullptr,
      0
    ))
      continue;

    hub->ports[i].status.status |= USB_PORT_STATUS_POWER;
  }

  /* Delay until power on the hub has stabilized */
  mdelay(hub->hubdesc.power_stabilize_delay_2ms * 2);

  return error;
}

/*!
 * @brief Allocate and initialize a generic USB hub
 *
 * TODO: create our own root hub configuration descriptor
 */
int create_usb_hub(usb_hub_t** phub, struct usb_hcd* hcd, enum USB_HUB_TYPE type, usb_hub_t* parent, usb_device_t* device, uint32_t portcount)
{
  int error;
  usb_hub_t* hub;
  dgroup_t* parent_group;
  char hubgroupname[4] = { NULL};

  if (!device)
    return -1;

  /* Set null just in case */
  if (phub)
    *phub = nullptr;

  if (sfmt(hubgroupname, "%d", device->hub_port))
    return -1;

  hub = kmalloc(sizeof(*hub));

  if (!hub)
    return -1;

  memset(hub, 0, sizeof(*hub));

  parent_group = _root_usbhub_group;

  if (parent)
    parent_group = parent->devgroup;

  hub->device = device;
  hub->parent = parent;
  hub->hcd = hcd;
  hub->type = type;
  hub->portcount = portcount;

  /* Register a dev group for this hub */
  hub->devgroup = register_dev_group(DGROUP_TYPE_USB, hubgroupname, DGROUP_FLAG_BUS, parent_group->node);

  if (!hub->devgroup)
    goto destroy_and_exit;

  /* Check if we know the amount of ports on this hub */
  if (portcount) {
    hub->ports = kmalloc(portcount * sizeof(usb_port_t));

    if (!hub->ports)
      goto destroy_and_exit;

    memset(hub->ports, 0, portcount * sizeof(usb_port_t));
  }

  /* Creation is done, set the pointer and start init */
  if (phub) *phub = hub;

  error = _init_usb_hub(hub);

  if (error)
    goto destroy_and_exit;

  return 0;

destroy_and_exit:
  destroy_usb_hub(hub);

  if (phub) *phub = nullptr;
  return -1;
}

/*!
 * @brief Deallocate a generic USB hub
 *
 */
void destroy_usb_hub(usb_hub_t* hub)
{
  kernel_panic("TODO: destroy_usb_hub");
}

int usb_hub_submit_hc_ctl(usb_hub_t* hub, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len)
{
  if (!hub)
    return -1;

  return _usb_submit_ctl(hub->hcd, hub->device, USB_HC_PORT, hub->device->dev_addr, hub->device->hub_port,
      reqtype, req, value, idx, len, respbuf, respbuf_len);
}

static int usb_hub_get_portsts(usb_hub_t* hub, uint32_t i, usb_port_status_t* status)
{
  return usb_device_submit_ctl(
    hub->device,
    USB_TYPE_STANDARD | USB_TYPE_DEV_IN,
    USB_REQ_GET_STATUS,
    0,
    i+1,
    sizeof(usb_port_status_t),
    status,
    sizeof(usb_port_status_t)
  );
}

static inline void _get_usb_speed(usb_hub_t* hub, usb_port_t* port, enum USB_SPEED *speed)
{
  if ((port->status.status & USB_PORT_STATUS_POWER) == 0) {
    *speed = hub->device->speed;
    return;
  }

  if (port->status.status & USB_PORT_STATUS_LOW_SPEED) {
    *speed = USB_LOWSPEED;
    printf("Lowspeed device\n");
  } else if (port->status.status & USB_PORT_STATUS_HIGH_SPEED) {
    *speed = USB_HIGHSPEED;
    printf("Highspeed device\n");
  } else {
    *speed = USB_FULLSPEED;
    printf("Fullspeed device\n");
  }
}

static inline int _reset_port(usb_hub_t* hub, uint32_t i)
{
  int error;
  usb_port_t* port;

  error = usb_device_submit_ctl(hub->device, USB_TYPE_CLASS | USB_TYPE_OTHER_OUT, USB_REQ_SET_FEATURE, USB_FEATURE_PORT_RESET, i+1, NULL, NULL, NULL);

  if (error)
    return error;

  port = &hub->ports[i];

  for (uint32_t i = 0; i < 16; i++) {
    error = usb_hub_get_portsts(hub, i, &port->status);

    if (error)
      return error;

    if ((port->status.change & USB_PORT_STATUS_RESET) == USB_PORT_STATUS_RESET || (port->status.status & USB_PORT_STATUS_RESET) == 0)
      break;

    error = -KERR_NODEV;
  }

  if (error)
    return error;

  error = usb_device_submit_ctl(hub->device, USB_TYPE_CLASS | USB_TYPE_OTHER_OUT, USB_REQ_CLEAR_FEATURE, USB_FEATURE_C_PORT_RESET, i+1, NULL, NULL, NULL);

  if (error)
    return error;

  /* Delay a bit to give hardware time to chill */
  mdelay(250);
  return 0;
}

static inline int _handle_device_connect(usb_hub_t* hub, uint32_t i)
{
  int error;
  usb_port_t* port;
  enum USB_SPEED speed;
  char namebuf[11] = { NULL };

  if (i >= hub->portcount)
    return -1;

  port = &hub->ports[i];

  /* Reset the port */
  error = _reset_port(hub, i);

  if (error)
    return error;

  /* Update the status after the reset */
  usb_hub_get_portsts(hub, i, &port->status);

  /* Get the device speed */
  _get_usb_speed(hub, port, &speed);

  /* Format the device name */
  sfmt(namebuf, "usbdev%d", i); 

  /* Create the device backend structs */
  port->device = create_usb_device(hub->hcd, hub, speed, i, namebuf);

  /* Bruh */
  if (!port->device)
    return -1;

  /* Try and get the device address to the HCD and get the descriptor from it */
  init_usb_device(port->device);

  return 0;
}

static inline int _handle_device_disconnect(usb_hub_t* hub, uint32_t i)
{
  return 0;
}

/*!
 * @brief: Called when we encounter a connected device during hub enumeration
 *
 * When there is not yet a device at this port, we create it
 */
static inline int _handle_port_connection(usb_hub_t* hub, uint32_t i)
{
  /* Clear the connected change status */
  usb_device_submit_ctl(hub->device, USB_TYPE_CLASS, USB_REQ_CLEAR_FEATURE, USB_FEATURE_C_PORT_CONNECTION, i+1, NULL, NULL, NULL);

  if (usb_port_is_connected(&hub->ports[i]))
    return _handle_device_connect(hub, i);

  return _handle_device_disconnect(hub, i);
}

/*!
 * @brief: Check a particular hub for devices
 */
int usb_hub_enumerate(usb_hub_t* hub)
{
  int error;
  usb_port_t* c_port;

  for (uint32_t i = 0; i < hub->portcount; i++) {
    printf("%s: Scanning port %d...\n", hub->device->device->name, i);
    c_port = &hub->ports[i];

    error = usb_hub_get_portsts(hub, i, &c_port->status);

    if (error)
      continue;

    printf("Got port status:\n\tenabled: %s\n\tconnected: %s\n\tpower: %s\n", 
        (c_port->status.status & USB_PORT_STATUS_ENABLE) ? "yes" : "no",
        (c_port->status.status & USB_PORT_STATUS_CONNECTION) ? "yes" : "no",
        (c_port->status.status & USB_PORT_STATUS_POWER) ? "yes" : "no"
        );

    if (usb_port_is_uninitialised(c_port) || usb_port_has_connchange(c_port))
      error = _handle_port_connection(hub, i);
  }

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
int unregister_usb_hcd(usb_hcd_t* hcd)
{
  kerror_t error;

  if (!_root_usbhub_group)
    return -1;

  /* Stop the controller */
  if (hcd->hw_ops && hcd->hw_ops->hcd_stop)
    hcd->hw_ops->hcd_stop(hcd);

  error = device_unregister(hcd->pci_device->dev);

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

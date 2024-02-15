#ifndef __ANIVA_DEV_DEVICE__
#define __ANIVA_DEV_DEVICE__

#include "dev/core.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "sync/mutex.h"
#include "system/acpi/tables.h"
#include <libk/stddef.h>

/*
 * Aniva device model
 *
 * Everything we do as a kernel either has effect on os is affected by the hardware. On most modern
 * systems we can always devide the hardware into groups and subgroups: devices. This is the lowest point
 * of abstraction for the kernel to talk to hardware. The kernel sees its underlying system as a tree of
 * devices that depend on each other, need different power management, or simply provide some kind of 
 * service to the software. It's our job as the kernel to keep every device seperated and controlled in
 * such a way that access is as unobstructed as can be, while keeping the options for abstraction maximized.
 * In an ideal world, there should be no point in time where userspace has to worry about any kind of
 * hardware issue. It just needs to do it's thing and request it's needs while the big strong kernel
 * defends it from the scary and unpredictable hardware.
 *
 * Leading up to userspace, there are a few sublayers that make up the entire abstraction layer for the 
 * hardware (HAL). Starting from raw hardware, we get:
 * - Physical device
 * - Firmware on the device / endpoint for any device driver
 * - The device driver (Either inhouse or external)
 * - The device object somewhere in the device tree
 * - Kernel API that covers the devices various subclasses
 * - System libraries that provide easy access to kernel APIs
 * - The wild west of userspace
 * Since a single device may do different things, it can have different 'subsystem endpoints' attached
 * to it. This simply means that when there is a call to the device through a specific subsystem, we
 * know where to go. These endpoints must be implemented by the device drivers.
 *
 * Let's take a simple example of a userspace program that wants to paint a singular pixel.
 * It might make a call to a graphics library to draw a pixel, which will eventually land in kernel-space
 * to invoke the appropriate API. The chain might look something like this:
 * 
 * < System call -> subsystem parse -> device parse (we either choose the best fitting device, or use a specified device)
 * -> device call -> driver code handles endpoint -> return all the way back to userspace with the result of our call >
 *
 * A program that's a bit smarter might do something like:
 * 
 * < Call graphics device to draw the pixel -> return >
 *
 * But we can't assume every program is as smart as this. The option should be there for direct device calls 
 * (It's ofcourse not advised) in order to enable freedom on the system.
 */

struct oss_obj;
struct device;
struct dgroup;
struct device_endpoint;
struct aniva_driver;
struct dev_manifest_t;

/* Device that is managed and backed entirely by software */
#define DEV_FLAG_SOFTDEV    0x00000001
/* Device that controlls other devices (Think AHCI controller or EHCI, OHCI or XHCI stuff) */
#define DEV_FLAG_CONTROLLER 0x00000002
/*
 * Is this device powered on? (When powered on, the device can still be in sleep mode for example. 
 * This needs to also get checked) 
 */
#define DEV_FLAG_POWERED    0x00000004
/* A bus device can have multiple devices that it manages */
#define DEV_FLAG_BUS        0x00000008
#define DEV_FLAG_ERROR      0x00000010
/* TODO: more device flags */

/*
 * The device
 *
 * (TODO)
 * (TODO)
 *
 * The one and only. We construct a device tree by sorting our drivers
 * and making them available through a path system. Drivers themselves are able to register
 * devices to themselves, which makes every device 'access' in userspace (or even
 * kernelspace for that matter) go through their respective driver.
 *
 * Device registering:
 * What happens when we attach a PCI bus device but we also want to register it's children?
 * well, we can simply attach the children to a grouped 'bridge node' which acts as a collector
 * of the children of the actual bus device. Let's suppose a PCI bus device is called 'sick_pci_bridge'.
 * Accessing this bridge as a device is easy, it will just be attached to 'Dev/sick_pci_bridge'. What happens
 * if it turns out it has children though? These can be grouped under the 'pci' node. The bus device will
 * be given a bus number, which we can use to access the correct bus group under 'Dev/pci'. For this example,
 * let's say there is a device, 'epic_dev' on the pci bus and the bus number is 3. The total path to access the
 * device on the bus will then become:
 *
 * 'Dev/pci/3/epic_dev'
 * 
 * This way we can easily distinguish devices from eachother. What happens when there are nested bus devices inside 
 * a bus? They just get a new bus number, together with a new node to match that number. So let's say there is a
 * nested bus on 'sick_pci_bridge' which is called 'nested_bridge'. It is the only other bridge/bus device here, so
 * it get's bus number 0. There are now three entries on the group node for 'sick_pci_bridge'. They are:
 *
 * 'Dev/pci/3/epic_dev'
 * 'Dev/pci/3/nested_bridge'
 * 'Dev/pci/3/0'
 */
typedef struct device {
  const char* name;
  /* Driver that is responsible for management of this device */
  struct dev_manifest* parent;
  struct oss_obj* obj;
  /* If this device is a bus, this node contains it's children */
  struct dgroup* bus_group;

  void* private;
  mutex_t* lock;

  /* Handle to the ACPI stuff if this device has that */
  acpi_handle_t acpi_dev;

  uint32_t flags;
  /* Should be initialized by the device driver. Remains constant throughout the entire lifetime of the device */
  uint32_t endpoint_count;
  struct device_endpoint* endpoints;
} device_t;

static inline bool dev_has_endpoint(device_t* device, enum ENDPOINT_TYPE type)
{
  if (!device->endpoints)
    return false;

  for (uint32_t i = 0; i < device->endpoint_count; i++)
    if (device->endpoints[i].type == type)
      return true;

  return false;
}

static inline struct device_endpoint* dev_get_endpoint(device_t* device, enum ENDPOINT_TYPE type)
{
  if (!device->endpoints)
    return nullptr; 

  for (uint32_t i = 0; i < device->endpoint_count; i++) {
    if (device->endpoints[i].type == type)
      return &device->endpoints[i];
  }

  return nullptr;
}

/*!
 * @brief: Check if this device has a bus group linked to it
 */
static inline bool dev_is_bus(device_t *dev)
{
  return dev->bus_group != nullptr;
}


void init_devices();

device_t* create_device(struct aniva_driver* parent, char* name);
device_t* create_device_ex(struct aniva_driver* parent, char* name, void* priv, uint32_t flags, struct device_endpoint* eps, uint32_t ep_count);
void destroy_device(device_t* device);

int device_read(device_t* dev, void* buffer, uintptr_t offset, size_t size);
int device_write(device_t* dev, void* buffer, uintptr_t offset, size_t size);

int device_power_on(device_t* dev);
int device_on_remove(device_t* dev);
int device_suspend(device_t* dev);
int device_resume(device_t* dev);

uintptr_t device_message(device_t* dev, dcc_t code);
uintptr_t device_message_ex(device_t* dev, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

int device_get_group(device_t* dev, struct dgroup** group);

void devices_debug();

int device_register(device_t* dev);
/* TODO: */
int device_unregister();

device_t* open_device(const char* path);
int close_device(device_t* dev);

int device_add_group(dgroup_t* group, struct oss_node* node);

#endif // !__ANIVA_DEV_DEVICE__

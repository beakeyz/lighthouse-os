#ifndef __ANIVA_DEV_DEVICE__
#define __ANIVA_DEV_DEVICE__

#include "dev/core.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "libk/flow/error.h"
#include "sync/mutex.h"
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
struct pci_device;
struct acpi_device;
struct device_endpoint;
struct aniva_driver;
struct drv_manifest;

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
/* Device without driver attached and managed by the system core */
#define DEV_FLAG_CORE       0x00000020
#define DEV_FLAG_ENABLED    0x00000040
/* TODO: more device flags */

typedef bool (*DEVICE_ITTERATE)(struct device* dev);

/*
 * Types to specify devices
 * TODO: use
 */
enum DEVICE_TYPE {
  DEVICE_TYPE_GENERIC = 0,
  DEVICE_TYPE_ACPI,
  DEVICE_TYPE_DISK,
  DEVICE_TYPE_VIDEO,
  DEVICE_TYPE_HID,
  DEVICE_TYPE_PCI,
  DEVICE_TYPE_USBCLASS,
  DEVICE_TYPE_USBHCD,
};

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
  struct drv_manifest* driver;
  struct oss_obj* obj;
  /* If this device is a bus, this node contains it's children */
  struct dgroup* bus_group;

  /* Private field for drivers to use */
  void* private;

  mutex_t* lock;
  mutex_t* ep_lock;

  /* Handle to the ACPI stuff if this device has that */
  struct acpi_device* acpi_dev;
  struct pci_device* pci_dev;

  uint32_t flags;
  /* Should be initialized by the device driver. Remains constant throughout the entire lifetime of the device */
  uint32_t endpoint_count;
  struct device_endpoint* endpoints;
} device_t;

static inline void* device_unwrap(device_t* device)
{
  return device->private;
}

/*
 * endpoint.c
 */
extern struct device_endpoint* device_get_endpoint(device_t* device, enum ENDPOINT_TYPE type);
extern kerror_t device_unimplement_endpoint(device_t* device, enum ENDPOINT_TYPE type);
extern kerror_t device_implement_endpoint(device_t* device, enum ENDPOINT_TYPE type, void* ep, size_t ep_size);
extern kerror_t device_implement_endpoints(device_t* device, struct device_endpoint* endpoints);

static inline bool device_has_endpoint(device_t* device, enum ENDPOINT_TYPE type)
{
  return (device_get_endpoint(device, type) != nullptr);
}

/*!
 * @brief: Check if this device has a bus group linked to it
 */
static inline bool device_is_bus(device_t *dev)
{
  return dev->bus_group != nullptr;
}

/* Subsystem */
void init_devices();
void init_hw();
void debug_devices();

/* Object management */
device_t* create_device(struct drv_manifest* parent, char* name, void* priv);
device_t* create_device_ex(struct drv_manifest* parent, char* name, void* priv, uint32_t flags, struct device_endpoint* eps);
void destroy_device(device_t* device);

/* Device registering */
int device_register(device_t* dev, struct dgroup* group);
int device_register_to_bus(device_t* dev, device_t* busdev);
int device_unregister(device_t* dev);
int device_move(device_t* dev, struct dgroup* newgroup);
int device_move_to_bus(device_t* dev, device_t* newbus);

/* Device-group interfacing */
int device_get_group(device_t* dev, struct dgroup** group);
int device_node_add_group(struct oss_node* node, struct dgroup* group);
int device_for_each(struct dgroup* root, DEVICE_ITTERATE callback);

/* DRV-API functions */
kerror_t device_alloc_memory(device_t* dev, uintptr_t start, size_t size);
kerror_t device_alloc_memory_range(device_t* dev, size_t size, void** b_out);
kerror_t device_alloc_irq(device_t* dev, uint32_t vec, uint32_t flags, void* handler, void* ctx, const char* name);
kerror_t device_alloc_io(device_t* dev, uint32_t start, uint32_t size);

kerror_t device_dealloc_memory(device_t* dev, uintptr_t start, size_t size);
kerror_t device_dealloc_irq(device_t* dev, uint32_t vec);
kerror_t device_dealloc_io(device_t* dev, uint32_t start, uint32_t size);

kerror_t device_bind_driver(device_t* dev, struct drv_manifest* driver);
bool device_has_driver(device_t* dev);
kerror_t device_bind_acpi(device_t* dev, struct acpi_device* acpidev);
kerror_t device_bind_pci(device_t* dev, struct pci_device* pcidev);
kerror_t device_rename(device_t* dev, const char* newname);

/* Misc */
device_t* device_open(const char* path);
int device_close(device_t* dev);
int device_getinfo(device_t* dev, DEVINFO* binfo);


/* I/O */
int device_read(device_t* dev, void* buffer, uintptr_t offset, size_t size);
int device_write(device_t* dev, void* buffer, uintptr_t offset, size_t size);

uintptr_t device_message(device_t* dev, dcc_t code);
uintptr_t device_message_ex(device_t* dev, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

/* Power management */
int device_power_on(device_t* dev);
int device_on_remove(device_t* dev);
int device_suspend(device_t* dev);
int device_resume(device_t* dev);

/* Device state management */
int device_enable(device_t* dev);
int device_disable(device_t* dev);

static inline bool device_is_enabled(device_t* dev)
{
  if (!dev)
    return false;

  return ((dev->flags & DEV_FLAG_ENABLED) == DEV_FLAG_ENABLED);
}

#endif // !__ANIVA_DEV_DEVICE__

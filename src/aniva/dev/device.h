#ifndef __ANIVA_DEV_DEVICE__
#define __ANIVA_DEV_DEVICE__

#include "dev/core.h"
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
struct aniva_driver;
struct dev_manifest_t;

typedef struct device_ops {
  int (*d_read)(struct device* device, void* buffer, size_t size, uintptr_t offset);
  int (*d_write)(struct device* device, void* buffer, size_t size, uintptr_t offset);

  /* 'pm' opperations which are purely software, except if we support actual PM */
  int (*d_remove)(struct device* device);
  int (*d_suspend)(struct device* device);
  int (*d_resume)(struct device* device);

  uintptr_t (*d_msg)(struct device* device, dcc_t code);
  uintptr_t (*d_msg_ex)(struct device* device, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);
} device_ops_t;

/* Device that is managed and backed entirely by software */
#define DEV_FLAG_SOFTDEV 0x00000001
/* Device that controlls other devices (Think AHCI controller or EHCI, OHCI or XHCI stuff) */
#define DEV_FLAG_CONTROLLER 0x00000002
/*
 * Is this device powered on? (When powered on, the device can still be in sleep mode for example. 
 * This needs to also get checked) 
 */
#define DEV_FLAG_POWERED 0x00000004
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
 */
typedef struct device {
  const char* device_path;
  /* Driver that is responsible for management of this device */
  struct dev_manifest* parent;
  /* Driver this device is linked to. (NOTE: parent and link can be the same, but don't have to be) */
  struct dev_manifest* link;
  struct oss_obj* obj;

  uint32_t flags;

  void* private;

  mutex_t* lock;
  device_ops_t* ops;
} device_t;

void init_device();

device_t* create_device(struct aniva_driver* parent, char* path);
device_t* create_device_ex(struct aniva_driver* parent, char* path, uint32_t flags, device_ops_t* ops);
void destroy_device(device_t* device);

int device_read(device_t* dev, void* buffer, size_t size, uintptr_t offset);
int device_write(device_t* dev, void* buffer, size_t size, uintptr_t offset);

int device_remove(device_t* dev);
int device_suspend(device_t* dev);
int device_resume(device_t* dev);

uintptr_t device_message(device_t* dev, dcc_t code);
uintptr_t device_message_ex(device_t* dev, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

bool device_is_generic(device_t* device);

#endif // !__ANIVA_DEV_DEVICE__

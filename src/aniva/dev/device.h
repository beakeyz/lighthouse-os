#ifndef __ANIVA_DEV_DEVICE__
#define __ANIVA_DEV_DEVICE__

#include "dev/core.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct vobj;
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
  struct vobj* obj;

  uint32_t flags;

  void* private;

  mutex_t* lock;
  device_ops_t* ops;
} device_t;

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

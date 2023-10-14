#ifndef __ANIVA_DEV_DEVICE__
#define __ANIVA_DEV_DEVICE__

#include "dev/core.h"
#include <libk/stddef.h>

struct device;
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
  struct dev_manifest* parent;

  device_ops_t* ops;
} device_t;

device_t* create_device(char* path);
void destroy_device(device_t* device);


#endif // !__ANIVA_DEV_DEVICE__

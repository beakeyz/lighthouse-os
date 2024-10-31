#ifndef __ANIVA_DEV_RAMDISK__
#define __ANIVA_DEV_RAMDISK__

#include "dev/disk/device.h"

struct device;
struct driver;

extern volume_device_t* create_generic_ramdev(size_t size);
extern volume_device_t* create_generic_ramdev_at(uintptr_t address, size_t size);
extern bool destroy_generic_ramdev(volume_device_t* device);

extern int ramdisk_read(struct device* device, struct driver* driver, uintptr_t offset, void* buffer, size_t size);
extern int ramdisk_write(struct device* device, struct driver* driver, uintptr_t offset, void* buffer, size_t size);

#endif // !__ANIVA_DEV_RAMDISK__

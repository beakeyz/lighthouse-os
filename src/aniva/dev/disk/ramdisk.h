#ifndef __ANIVA_DEV_RAMDISK__
#define __ANIVA_DEV_RAMDISK__

#include "dev/disk/generic.h"

struct device;
struct drv_manifest;

extern disk_dev_t* create_generic_ramdev(size_t size);
extern disk_dev_t* create_generic_ramdev_at(uintptr_t address, size_t size);
extern bool destroy_generic_ramdev(disk_dev_t* device);

extern int ramdisk_read(struct device* device, struct drv_manifest* driver, disk_offset_t offset, void* buffer, size_t size);
extern int ramdisk_write(struct device* device, struct drv_manifest* driver, disk_offset_t offset, void* buffer, size_t size);

#endif // !__ANIVA_DEV_RAMDISK__

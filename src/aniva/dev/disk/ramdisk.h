#ifndef __ANIVA_DEV_RAMDISK__
#define __ANIVA_DEV_RAMDISK__

#include "dev/disk/generic.h"

struct device;

extern disk_dev_t* create_generic_ramdev(size_t size);
extern disk_dev_t* create_generic_ramdev_at(uintptr_t address, size_t size);
extern bool destroy_generic_ramdev(disk_dev_t* device);

extern int ramdisk_read(struct device* device, void* buffer, disk_offset_t offset, size_t size);
extern int ramdisk_write(struct device* device, void* buffer, disk_offset_t offset, size_t size);

#endif // !__ANIVA_DEV_RAMDISK__

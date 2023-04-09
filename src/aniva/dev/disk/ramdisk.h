#ifndef __ANIVA_DEV_RAMDISK__
#define __ANIVA_DEV_RAMDISK__

#include "dev/disk/generic.h"

extern generic_disk_dev_t* create_generic_ramdev(size_t size);
extern generic_disk_dev_t* create_generic_ramdev_at(uintptr_t address, size_t size);
extern bool destroy_generic_ramdev(generic_disk_dev_t* device);

extern int ramdisk_read(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset);
extern int ramdisk_write(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset);

#endif // !__ANIVA_DEV_RAMDISK__

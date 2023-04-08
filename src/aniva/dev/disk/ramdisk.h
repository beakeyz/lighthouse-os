#ifndef __ANIVA_DEV_RAMDISK__
#define __ANIVA_DEV_RAMDISK__

#include "dev/disk/generic.h"

extern generic_disk_dev_t* create_generic_ramdev(void* start_addr, size_t size);

extern int ramdisk_read(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset);
extern int ramdisk_write(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset);

#endif // !__ANIVA_DEV_RAMDISK__

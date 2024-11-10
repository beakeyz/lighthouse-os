#ifndef __ANIVA_DEV_RAMDISK__
#define __ANIVA_DEV_RAMDISK__

#include "dev/disk/device.h"

volume_device_t* create_generic_ramdev(size_t size);
volume_device_t* create_generic_ramdev_at(uintptr_t address, size_t size);

#endif // !__ANIVA_DEV_RAMDISK__

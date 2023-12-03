#ifndef __ANIVA_NVIDIA_THERM_CORE__
#define __ANIVA_NVIDIA_THERM_CORE__

#include "drivers/video/nvidia/device/subdev.h"
#include <drivers/video/nvidia/device/device.h>

typedef struct nv_therm_subdev {
  int test;
} nv_therm_subdev_t;

int g84_therm_create(nv_device_t* device, enum NV_SUBDEV_TYPE type, void** subdev);

#endif // !__ANIVA_NVIDIA_THERM_CORE__

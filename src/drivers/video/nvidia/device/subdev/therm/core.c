#include "core.h"
#include "drivers/video/nvidia/device/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "mem/heap.h"

nv_subdev_therm_t* create_generic_therm_subdev(struct nv_subdev* parent, struct nv_therm_ops* ops)
{
  nv_subdev_therm_t* therm;

  therm = kmalloc(sizeof(*therm));

  if (!therm)
    return nullptr;

  memset(therm, 0, sizeof(*therm));

  therm->parent = parent;
  therm->ops = ops;

  return therm;
}

void destroy_generic_therm_subdev(nv_subdev_therm_t* therm)
{
  kfree(therm);
}

nv_subdev_therm_t* nvdev_get_therm(nv_device_t* device)
{
  nv_subdev_t* subdev = nvdev_get_subdev(device, NV_SUBDEV_THERM);

  if (!subdev)
    return nullptr;

  return subdev->priv;
}

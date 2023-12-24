#include "core.h"
#include "drivers/video/nvidia/device/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include "internal.h"

nv_subdev_fuse_t* create_nv_fuse_subdev(nv_subdev_t* subdev, struct nv_subdev_fuse_ops* ops)
{
  nv_subdev_fuse_t* fuse;

  fuse = kmalloc(sizeof(*fuse));

  if (!fuse)
    return nullptr;
  
  fuse->lock = create_mutex(NULL);
  fuse->ops = ops;
  fuse->subdev = subdev;

  return fuse;
}

void destroy_nv_fuse_subdev(nv_subdev_fuse_t* fuse)
{
  destroy_mutex(fuse->lock);
  kfree(fuse);
}

uint32_t nv_sd_fuse_read(nv_subdev_fuse_t* fuse, uint32_t addr)
{
  return fuse->ops->f_read(fuse, addr);
}

nv_subdev_fuse_t* nvdev_get_fuse(nv_device_t* device)
{
  nv_subdev_t* subdev = nvdev_get_subdev(device, NV_SUBDEV_FUSE);

  if (!subdev)
    return nullptr;

  return subdev->priv;
}

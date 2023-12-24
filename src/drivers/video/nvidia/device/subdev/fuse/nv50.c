#include "core.h"
#include "drivers/video/nvidia/device/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "drivers/video/nvidia/device/subdev/fuse/internal.h"
#include "libk/flow/error.h"
#include "sync/mutex.h"

uint32_t nv50_fuse_read(nv_subdev_fuse_t* fuse, uint32_t addr)
{
  uint32_t fuse_enable, ret;
  nv_device_t* nvdev = fuse->subdev->parent;

  mutex_lock(fuse->lock);

  fuse_enable = nvd_mask(nvdev, 0x001084, 0x800, 0x800);
  ret = nvd_rd32(nvdev, 0x021000 + addr);
  nvd_wr32(nvdev, 0x001084, fuse_enable);

  mutex_unlock(fuse->lock);

  return ret;
}

nv_subdev_fuse_ops_t nv50_fuse_ops = {
  .f_read = nv50_fuse_read,
};

int nv50_fuse_init(nv_subdev_t* subdev)
{
  /* Nothing to be done */
  return 0;
}

void nv50_fuse_destroy(nv_subdev_t* subdev)
{
}

nv_subdev_ops_t nv50_fuse_subdev_ops = {
  .f_init = nv50_fuse_init,
  .f_destroy = nv50_fuse_destroy,
};

int nv50_fuse_create(nv_device_t* nvdev, enum NV_SUBDEV_TYPE type, void** subdev)
{
  nv_subdev_t* dev;
  nv_subdev_fuse_t* fuse;

  /* NOTE: We set subdev later, since we need to allocate this before we allocate the subdev */
  fuse = create_nv_fuse_subdev(NULL, &nv50_fuse_ops);

  if (!fuse)
    return -1;

  dev = create_nv_subdev(nvdev, type, &nv50_fuse_subdev_ops, fuse);

  if (!dev)
    goto dealloc_and_exit;

  /* TODO: init shit */
  fuse->subdev = dev;

  *subdev = dev;
  return 0;

dealloc_and_exit:
  destroy_nv_fuse_subdev(fuse);
  return -1;
}


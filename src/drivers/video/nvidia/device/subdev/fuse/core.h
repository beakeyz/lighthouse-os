#ifndef __NVIDIA_SUBDEV_FUSE__
#define __NVIDIA_SUBDEV_FUSE__

#include "drivers/video/nvidia/device/subdev.h"
#include "sync/mutex.h"

#define nv_subdev_fuse(subdev) \
  (struct nv_subdev_fuse*)((subdev)->priv)

struct nv_device;
struct nv_subdev_fuse_ops;

typedef struct nv_subdev_fuse {
  struct nv_subdev_fuse_ops* ops;
  nv_subdev_t* subdev;
  mutex_t* lock;
} nv_subdev_fuse_t;

nv_subdev_fuse_t* create_nv_fuse_subdev(struct nv_device* device, struct nv_subdev_fuse_ops* ops);

uint32_t nv_sd_fuse_read(nv_subdev_fuse_t* fuse, uint32_t addr);

int nv50_fuse_create(struct nv_device* nvdev, enum NV_SUBDEV_TYPE type, void** subdev);

#endif // !__NVIDIA_SUBDEV_FUSE__

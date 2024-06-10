#ifndef __NVIDIA_SUBDEV_FUSE_INTERNAL__
#define __NVIDIA_SUBDEV_FUSE_INTERNAL__

#include "drivers/video/nvidia/device/subdev/fuse/core.h"
#include <libk/stddef.h>

typedef struct nv_subdev_fuse_ops {
    uint32_t (*f_read)(nv_subdev_fuse_t* fuse, uint32_t addr);
} nv_subdev_fuse_ops_t;

#endif // !__NVIDIA_SUBDEV_FUSE_INTERNAL__

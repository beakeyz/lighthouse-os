#include "core.h"
#include "drivers/video/nvidia/device/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "drivers/video/nvidia/device/subdev/fuse/internal.h"
#include "sync/mutex.h"

uint32_t nv50_fuse_read(nv_subdev_fuse_t* fuse, uint32_t addr)
{
    uint32_t fuse_enable, ret;
    nv_device_t* nvdev = fuse->subdev->parent;

    mutex_lock(fuse->lock);

    fuse_enable = nvd_mask(nvdev, 0x001084, 0x800, 0x800);
    ret = nvdev_rd32(nvdev, 0x021000 + addr);
    nvdev_wr32(nvdev, 0x001084, fuse_enable);

    mutex_unlock(fuse->lock);

    return ret;
}

nv_subdev_fuse_ops_t nv50_fuse_ops = {
    .f_read = nv50_fuse_read,
};

int nv50_fuse_create(nv_device_t* nvdev, enum NV_SUBDEV_TYPE type, void** subdev)
{
    nv_subdev_fuse_t* fuse;

    if (type != NV_SUBDEV_FUSE)
        return -1;

    /* NOTE: We set subdev later, since we need to allocate this before we allocate the subdev */
    fuse = create_nv_fuse_subdev(nvdev, &nv50_fuse_ops);

    if (!fuse)
        return -1;

    *subdev = fuse->subdev;
    return 0;
}

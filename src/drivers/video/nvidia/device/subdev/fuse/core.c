#include "core.h"
#include "drivers/video/nvidia/device/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "internal.h"
#include "mem/heap.h"
#include "sync/mutex.h"

int fuse_init(nv_subdev_t* subdev)
{
    /* Nothing to be done */
    return 0;
}

void fuse_destroy(nv_subdev_t* subdev)
{
    nv_subdev_fuse_t* fuse = nv_subdev_fuse(subdev);

    destroy_mutex(fuse->lock);
    kfree(fuse);
}

nv_subdev_ops_t fuse_subdev_ops = {
    .f_init = fuse_init,
    .f_destroy = fuse_destroy,
};

/*!
 * @brief: Creates a subdevice and initializes fuse subdev fields
 */
nv_subdev_fuse_t* create_nv_fuse_subdev(nv_device_t* device, struct nv_subdev_fuse_ops* ops)
{
    nv_subdev_t* subdev;
    nv_subdev_fuse_t* fuse;

    fuse = kmalloc(sizeof(*fuse));

    if (!fuse)
        return nullptr;

    memset(fuse, 0, sizeof(*fuse));

    subdev = create_nv_subdev(device, NV_SUBDEV_FUSE, &fuse_subdev_ops, fuse);

    fuse->lock = create_mutex(NULL);
    fuse->ops = ops;
    fuse->subdev = subdev;

    return fuse;
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

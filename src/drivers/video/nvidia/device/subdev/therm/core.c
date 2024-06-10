#include "core.h"
#include "drivers/video/nvidia/device/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "mem/heap.h"

void destroy_therm_subdev(nv_subdev_t* subdev)
{
    nv_subdev_therm_t* therm = nv_subdev_therm(subdev);

    destroy_nv_therm_subdev(therm);
}

/*!
 * @brief: Initialize g84-specific thermal subdev stuff
 *
 * TODO: init fans
 */
int init_therm_subdev(nv_subdev_t* subdev)
{
    nv_subdev_therm_t* therm = subdev->priv;

    if (!therm || !therm->ops || !therm->ops->f_init)
        return -1;

    therm->ops->f_init(therm);
    return 0;
}

static nv_subdev_ops_t therm_subdev_ops = {
    .f_init = init_therm_subdev,
    .f_destroy = destroy_therm_subdev,
};

nv_subdev_therm_t* create_nv_therm_subdev(struct nv_device* device, struct nv_therm_ops* ops)
{
    nv_subdev_t* subdev;
    nv_subdev_therm_t* therm;

    therm = kmalloc(sizeof(*therm));

    if (!therm)
        return nullptr;

    memset(therm, 0, sizeof(*therm));

    subdev = create_nv_subdev(device, NV_SUBDEV_THERM, &therm_subdev_ops, therm);

    if (!subdev)
        goto dealloc_and_exit;

    therm->parent = subdev;
    therm->ops = ops;

    return therm;

dealloc_and_exit:
    kfree(therm);
    return nullptr;
}

void destroy_nv_therm_subdev(nv_subdev_therm_t* therm)
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

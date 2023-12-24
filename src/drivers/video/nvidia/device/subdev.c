#include "subdev.h"
#include "mem/heap.h"

int nv_subdev_init(nv_subdev_t* subdev)
{
  if (subdev && subdev->ops && subdev->ops->f_init)
    return subdev->ops->f_init(subdev);

  return -1;
}

int nv_subdev_fini(nv_subdev_t* subdev)
{
  if (subdev && subdev->ops && subdev->ops->f_fini)
    return subdev->ops->f_fini(subdev);

  return -1;
}

nv_subdev_t* create_nv_subdev(struct nv_device* nvdev, enum NV_SUBDEV_TYPE type, struct nv_subdev_ops* ops, void* priv)
{
  nv_subdev_t* subdev;

  if (!ops || !ops->f_destroy)
    return nullptr;

  subdev = kmalloc(sizeof(*subdev));

  if (!subdev)
    return nullptr;

  subdev->type = type;
  subdev->priv = priv;
  subdev->ops = ops;
  subdev->parent = nvdev;

  return subdev;
}

void destroy_nv_subdev(nv_subdev_t* subdev)
{
  if (!subdev)
    return;

  /* Must be present */
  subdev->ops->f_destroy(subdev);

  kfree(subdev);
}

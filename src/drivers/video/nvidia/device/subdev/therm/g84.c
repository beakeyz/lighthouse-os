#include "core.h"
#include "drivers/video/nvidia/device/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "libk/io.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"

/*!
 * @brief: Enable temp readings n shit
 */
void g84_init_therm(nv_subdev_therm_t* therm)
{
  uint32_t temp_ok;
  nv_device_t* nvdev = therm->parent->parent;

  temp_ok = nvd_rd32(nvdev, 0x1a8);

  log("TempOK: ");
  logln(to_string(temp_ok));

  if (temp_ok == 1) {
    nvd_mask(nvdev, 0x20008, 0x80008000, 0x80000000);
    nvd_mask(nvdev, 0x2000c, 0x80000003, 0x00000000);
    mdelay(20);
  }
}

/*
 * g84 uses the same pwm ops as NV50 cards
 */
static nv_therm_ops_t g84_therm_ops = {
  .f_init = g84_init_therm,
};

void g84_destroy(nv_subdev_t* subdev)
{
  /* Only destory thermal subdevices */
  if (!subdev || subdev->type != NV_SUBDEV_THERM)
    return;

  destroy_generic_therm_subdev(subdev->priv);
}

/*!
 * @brief: Initialize g84-specific thermal subdev stuff
 *
 * TODO: init fans
 */
int g84_init_subdev(nv_subdev_t* subdev)
{
  nv_subdev_therm_t* therm = subdev->priv;

  if (!therm || !therm->ops || !therm->ops->f_init)
    return -1;

  therm->ops->f_init(therm);
  return 0;
}

static nv_subdev_ops_t g84_subdev_ops = {
  .f_init = g84_init_subdev,
  .f_destroy = g84_destroy,
};

int g84_therm_create(nv_device_t* device, enum NV_SUBDEV_TYPE type, void** subdev)
{
  nv_subdev_t* dev;
  nv_subdev_therm_t* therm;

  /* This would surely suck */
  if (!device || type != NV_SUBDEV_THERM)
    return -1;

  dev = nullptr;
  therm = create_generic_therm_subdev(NULL, &g84_therm_ops);

  if (!therm)
    return -2;

  dev = create_nv_subdev(device, type, &g84_subdev_ops, therm);

  if (!dev)
    goto dealloc_and_exit;

  /*
   * TODO: initialize therm subdev fields
   */
  therm->parent = dev;

  *subdev = dev;
  return 0;

dealloc_and_exit:
  /* Try to destroy the nv therm */
  destroy_generic_therm_subdev(therm);
  return -1;
}

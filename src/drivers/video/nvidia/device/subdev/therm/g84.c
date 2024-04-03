#include "core.h"
#include "drivers/video/nvidia/device/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "drivers/video/nvidia/device/subdev/fuse/core.h"
#include "libk/io.h"
#include "libk/string.h"

/*!
 * @brief: Enable temp readings n shit
 */
void g84_init_therm(nv_subdev_therm_t* therm)
{
  uint32_t temp_ok;
  nv_subdev_fuse_t* fuse;
  nv_device_t* nvdev = therm->parent->parent;

  fuse = nvdev_get_fuse(nvdev);
  temp_ok = nv_sd_fuse_read(fuse, 0x1a8);

  print("TempOK: ");
  println(to_string(temp_ok));

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


int g84_therm_create(nv_device_t* device, enum NV_SUBDEV_TYPE type, void** subdev)
{
  nv_subdev_therm_t* therm;

  /* This would surely suck */
  if (!device || type != NV_SUBDEV_THERM)
    return -1;

  therm = create_nv_therm_subdev(device, &g84_therm_ops);

  if (!therm)
    return -2;

  *subdev = therm->parent;
  return 0;
}

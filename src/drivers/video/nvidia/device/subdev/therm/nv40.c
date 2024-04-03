#include "core.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include <drivers/video/nvidia/device/device.h>

/*
 * There are two 'styles' for managing the nv40 thermal subsystem: an old and a new style
 * Different chipsets use different styles, but like wtf
 */

enum NV40_THERM_STYLE {
  NV40_OLD_THERM_STYLE,
  NV40_NEW_THERM_STYLE,
  NV40_INVAL,
};

static inline enum NV40_THERM_STYLE _get_therm_style(nv_subdev_therm_t* therm)
{
  switch (therm->parent->parent->chipset) {
    case 0x43:
	case 0x44:
	case 0x4a:
	case 0x47:
		return NV40_OLD_THERM_STYLE;
	case 0x46:
	case 0x49:
	case 0x4b:
	case 0x4e:
	case 0x4c:
	case 0x67:
	case 0x68:
	case 0x63:
		return NV40_NEW_THERM_STYLE;
	default:
		return NV40_INVAL;
  }
}

static kerror_t nv40_fan_pwm_ctl(nv_subdev_therm_t* therm, uint32_t line, uint32_t divs, uint32_t duty)
{
  nv_subdev_t* subdev = therm->parent;

  switch (line) {
    case 2:
      nvd_mask(subdev->parent, 0x0010f0, 0x7fff7fff, (duty << 16) | divs);
      break;
    case 9:
      nvdev_wr32(subdev->parent, 0x0015f8, divs);
      nvd_mask(subdev->parent, 0x0015f4, 0x7fffffff, duty);
      break;
    default:
      return -KERR_INVAL;
  }

  return 0;
}

static void nv40_therm_init(nv_subdev_therm_t* therm)
{
  nv_subdev_t* subdev = therm->parent;
  enum NV40_THERM_STYLE therm_style = _get_therm_style(therm);

  printf("Initing nv40 therm...\n");

  switch (therm_style) {
    case NV40_OLD_THERM_STYLE:
      nvdev_wr32(subdev->parent, 0x15b0, 0xff);

      /* Chill */
      mdelay(20);

      /* Flush read yay (Do we need to do this?) */
      (void)nvdev_rd32(subdev->parent, 0x15b4);
    case NV40_NEW_THERM_STYLE:
      nvd_mask(subdev->parent, 0x15b8, 0x80000000, 0);
      nvdev_wr32(subdev->parent, 0x15b0, 0x80003fff);

      /* Chill */
      mdelay(20);

      /* Flush read yay (Do we need to do this?) */
      (void)nvdev_rd32(subdev->parent, 0x15b4);
      break;
    default:
      break;
  }

  printf("Trying to disable fans...\n");

  (void)nv40_fan_pwm_ctl(therm, 2, 0, 0);
  (void)nv40_fan_pwm_ctl(therm, 9, 0, 0);

  printf("Done!\n");
}

static nv_therm_ops_t nv40_therm_ops = {
  .f_init = nv40_therm_init,
};

int nv40_therm_create(struct nv_device* device, enum NV_SUBDEV_TYPE type, void** subdev)
{
  nv_subdev_therm_t* therm;

  /* This would surely suck */
  if (!device || type != NV_SUBDEV_THERM)
    return -1;

  therm = create_nv_therm_subdev(device, &nv40_therm_ops);

  if (!therm)
    return -2;

  *subdev = therm->parent;
  return 0;
}

#ifndef __ANIVA_NVIDIA_SUBDEV__
#define __ANIVA_NVIDIA_SUBDEV__

/*
 * Nvidia cards have multiple different subsystems that are all hosted on a 
 * single card. For this reason, just like linux, we are going to seperate these
 * services into different classes. This way we can have finer control over the cards
 * as a whole, because we can handle different subsystems for different card types 
 * seperately.
 */

enum NV_SUBDEV_TYPE {
  NV_SUBDEV_THERM = 0,
  NV_SUBDEV_FB,
  NV_SUBDEV_MMIO,
  NV_SUBDEV_BIOS,
  NV_SUBDEV_BAR,
  NV_SUBDEV_GPIO,
  NV_SUBDEV_I2C,

  NV_SUBDEV_COUNT = NV_SUBDEV_I2C
};

typedef struct nv_subdev {
  enum NV_SUBDEV_TYPE type;

  /* Every subdev has private handling code */
  void* priv;
} nv_subdev_t;

#endif // !__ANIVA_NVIDIA_SUBDEV__

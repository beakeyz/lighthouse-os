#ifndef __ANIVA_NVIDIA_SUBDEV__
#define __ANIVA_NVIDIA_SUBDEV__

#include <libk/stddef.h>

/*
 * Nvidia cards have multiple different subsystems that are all hosted on a 
 * single card. For this reason, just like linux, we are going to seperate these
 * services into different classes. This way we can have finer control over the cards
 * as a whole, because we can handle different subsystems for different card types 
 * seperately.
 */

struct nv_subdev_ops;
struct nv_device;

enum NV_SUBDEV_TYPE {
  NV_SUBDEV_GPIO,
  NV_SUBDEV_I2C,
  NV_SUBDEV_FUSE,
  NV_SUBDEV_MMIO,
  NV_SUBDEV_BIOS,
  NV_SUBDEV_BAR,
  NV_SUBDEV_THERM,
  NV_SUBDEV_FB,

  NV_SUBDEV_COUNT,
};

typedef struct nv_subdev {
  enum NV_SUBDEV_TYPE type;

  struct nv_device* parent;
  struct nv_subdev_ops* ops;

  /* Every subdev has private handling code */
  void* priv;
} nv_subdev_t;

nv_subdev_t* create_nv_subdev(struct nv_device* nvdev, enum NV_SUBDEV_TYPE type, struct nv_subdev_ops* ops, void* priv);
void destroy_nv_subdev(nv_subdev_t* subdev);

/*
 * Default opperations for a subdevice, specific to the subdevtype
 */
typedef struct nv_subdev_ops {
  /* Initialize subdev hardware */
  int (*f_init)(nv_subdev_t* subdev);
  /* Finalize subdev hardware */
  int (*f_fini)(nv_subdev_t* subdev);
  /* Destroy subdevice memory */
  void (*f_destroy)(nv_subdev_t* subdev);
} nv_subdev_ops_t;

int nv_subdev_init(nv_subdev_t* subdev);
int nv_subdev_fini(nv_subdev_t* subdev);


#endif // !__ANIVA_NVIDIA_SUBDEV__

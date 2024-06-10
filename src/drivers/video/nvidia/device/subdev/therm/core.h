#ifndef __ANIVA_NVIDIA_THERM_CORE__
#define __ANIVA_NVIDIA_THERM_CORE__

#include "drivers/video/nvidia/device/subdev.h"

#define nv_subdev_therm(subdev) \
    (struct nv_subdev_therm*)((subdev)->priv)

struct nv_device;
struct nv_subdev;
struct nv_therm_ops;

typedef struct nv_subdev_therm {
    struct nv_therm_ops* ops;
    struct nv_subdev* parent;
} nv_subdev_therm_t;

nv_subdev_therm_t* create_nv_therm_subdev(struct nv_device* device, struct nv_therm_ops* ops);
void destroy_nv_therm_subdev(nv_subdev_therm_t* therm);

extern int g84_therm_create(struct nv_device* device, enum NV_SUBDEV_TYPE type, void** subdev);
extern int nv40_therm_create(struct nv_device* device, enum NV_SUBDEV_TYPE type, void** subdev);

typedef struct nv_therm_ops {
    void (*f_init)(struct nv_subdev_therm*);
    void (*f_fini)(struct nv_subdev_therm*);

    int (*f_pwm_get)(struct nv_subdev_therm*, int line, uint32_t*, uint32_t*);
    int (*f_pwm_set)(struct nv_subdev_therm*, int line, uint32_t, uint32_t);

    int (*f_therm_get)(struct nv_subdev_therm*, int*);
} nv_therm_ops_t;

#endif // !__ANIVA_NVIDIA_THERM_CORE__

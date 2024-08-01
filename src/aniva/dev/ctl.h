#ifndef __ANIVA_DEVICE_CTL_H__
#define __ANIVA_DEVICE_CTL_H__

#include "devices/shared.h"

struct device;
struct drv_manifset;

typedef int (*f_device_ctl_imp_t)(struct device* device, struct drv_manifset* driver, enum DEVICE_CTLC code, void* buffer, size_t bsize);

/*
 * Single control endpoint for a device
 *
 * Device drivers let their devices expose a set of control codes, which allows for spreading device control over
 * multiple drivers
 */
typedef struct device_control {
    enum DEVICE_CTLC e_code;
    f_device_ctl_imp_t f_impl;
} device_control_t, device_ctl_t;

#endif // !__ANIVA_DEVICE_CTL_H__

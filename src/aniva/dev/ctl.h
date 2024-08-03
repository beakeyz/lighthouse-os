#ifndef __ANIVA_DEVICE_CTL_H__
#define __ANIVA_DEVICE_CTL_H__

#include "devices/shared.h"
#include <libk/stddef.h>

struct device;
struct drv_manifest;

typedef int (*f_device_ctl_t)(struct device* device, struct drv_manifest* driver, u64 offset, void* buffer, size_t bsize);

/* control can be rerouted by a different driver */
#define DEVICE_CTL_FLAG_UNIMPORTANT 0x0001
/*
 * The call count has been rolled over. The maximum amount of calls we
 * can count like this is 0x1ffff which is ~13000
 */
#define DEVICE_CTL_FLAG_CALL_ROLLOVER 0x0002

static inline u16 device_ctlflags_mask_status(u16 flags)
{
    return (flags & ~(DEVICE_CTL_FLAG_CALL_ROLLOVER));
}

struct device_ctl;

/*
 * Map which gets exported by a single device
 */
typedef struct device_ctlmap {
    u32 n_ctl;
    u32 ctlmap_sz;
    struct device* p_device;
    struct device_ctl* map;
} device_ctlmap_t;

device_ctlmap_t* create_device_ctlmap(struct device* dev);
void destroy_device_ctlmap(device_ctlmap_t* map);

int device_map_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code, struct drv_manifest* driver, f_device_ctl_t ctl, u16 flags);
int device_unmap_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code);

int device_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t bsize);

#endif // !__ANIVA_DEVICE_CTL_H__

#ifndef __ANIVA_DEVICE_CTL_H__
#define __ANIVA_DEVICE_CTL_H__

#include "lightos/api/device.h"
#include <libk/stddef.h>

struct device;
struct driver;

typedef int (*f_device_ctl_t)(struct device* device, struct driver* driver, u64 offset, void* buffer, size_t bsize);

/* control can be rerouted by a different driver */
#define DEVICE_CTL_FLAG_UNIMPORTANT 0x01
/*
 * The call count has been rolled over. The maximum amount of calls we
 * can count like this is 0x1ffff which is ~13000
 */
#define DEVICE_CTL_FLAG_CALL_ROLLOVER 0x02

static inline u16 device_ctlflags_mask_status(u16 flags)
{
    return (flags & ~(DEVICE_CTL_FLAG_CALL_ROLLOVER));
}

struct device_ctl;

enum DEVICE_CTLC device_ctl_get_code(struct device_ctl* ctl);
i32 device_ctl_get_callcount(struct device_ctl* ctl);

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

int device_map_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code, struct driver* driver, f_device_ctl_t ctl, u16 flags);
int device_unmap_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code);

int device_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t bsize);

int foreach_device_ctl(device_ctlmap_t* map, int (*f_cb)(struct device* dev, struct device_ctl* ctl, struct device_ctl** p_result), struct device_ctl** p_result);

#endif // !__ANIVA_DEVICE_CTL_H__

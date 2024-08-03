#include "ctl.h"
#include "devices/shared.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include <string.h>

typedef struct device_ctl {
    u16 b_flags;
    u16 s_n_calls;
    enum DEVICE_CTLC e_code;
    struct drv_manifest* p_driver;
    f_device_ctl_t f_impl;

    struct device_ctl* next;
} device_ctl_t;

/*!
 * @brief: Creates a device controlmap
 *
 * Allocates the memory for the controlmap and sets up the hashing shit
 */
device_ctlmap_t* create_device_ctlmap(struct device* dev)
{
    device_ctlmap_t* ret;

    if (!dev)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    ret->n_ctl = 0;
    /* This always needs to be a multiple of 2 */
    ret->ctlmap_sz = 0x10;
    ret->p_device = dev;
    ret->map = kmalloc(sizeof(device_ctl_t) * ret->ctlmap_sz);

    /* Fuck */
    if (!ret->map) {
        kfree(ret);
        return nullptr;
    }

    /* Clear the map */
    memset(ret->map, 0, sizeof(device_ctl_t) * ret->ctlmap_sz);

    return ret;
}

void destroy_device_ctlmap(device_ctlmap_t* map)
{
    kfree(map->map);
    kfree(map);
}

static inline u32 __hash_device_ctlc_to_idx(device_ctlmap_t* map, enum DEVICE_CTLC code)
{
    /* Since the max_idx is a multiple of 2, we can do this */
    return code & (map->ctlmap_sz - 1);
}

static inline int __devicemap_link_colliding_ctlcode(device_ctl_t* ctl, enum DEVICE_CTLC code, struct drv_manifest* driver, f_device_ctl_t impl, u16 flags)
{
    device_ctl_t** walker;
    device_ctl_t* new_ctl;
    device_ctl_t* next_ctl = nullptr;

    walker = &ctl;

    /* Check if this code is already in this link */
    while (*walker && (*walker)->e_code != code)
        walker = &(*walker)->next;

    /* Can't add duplicate control codes */
    if (*walker) {
        /* Control code isn't marked as unimportant, we can't replace it */
        if (((*walker)->b_flags & DEVICE_CTL_FLAG_UNIMPORTANT) != DEVICE_CTL_FLAG_UNIMPORTANT)
            return -KERR_DUPLICATE;

        /* This was an unimportant entry, we may replace it */
        new_ctl = *walker;
        next_ctl = new_ctl->next;
    } else
        /* Allocate a new control node */
        new_ctl = kmalloc(sizeof(*new_ctl));

    if (!new_ctl)
        return -KERR_NOMEM;

    if (!(*walker))
        /* Link this control code into the array at the end of the link */
        *walker = new_ctl;

    /* Populate with data */
    new_ctl->e_code = code;
    new_ctl->b_flags = flags;
    new_ctl->p_driver = driver;
    new_ctl->f_impl = impl;
    new_ctl->s_n_calls = 0;
    new_ctl->next = next_ctl;

    return 0;
}

int device_map_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code, struct drv_manifest* driver, f_device_ctl_t impl, u16 flags)
{
    u32 idx;
    device_ctl_t* p_ctl;

    if (!map || !impl)
        return -KERR_INVAL;

    idx = __hash_device_ctlc_to_idx(map, code);

    p_ctl = &map->map[idx];

    if (p_ctl->e_code)
        return __devicemap_link_colliding_ctlcode(p_ctl, code, driver, impl, flags);

    /* Simply put in the stuff */
    p_ctl->e_code = code;
    p_ctl->b_flags = flags;
    p_ctl->p_driver = driver;
    p_ctl->f_impl = impl;

    return 0;
}

static device_ctl_t* __device_map_get_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code)
{
    u32 idx;
    device_ctl_t* ret;

    if (!map || !code)
        return nullptr;

    /* Create an index from this code */
    idx = __hash_device_ctlc_to_idx(map, code);

    /* Find the control code at this location */
    ret = &map->map[idx];

    /* Find the control code we need */
    while (ret && ret->e_code != code)
        ret = ret->next;

    return ret;
}

static inline void __clean_ctlmap_entry(device_ctl_t* ctl, device_ctl_t* next)
{
    if (next) {
        /* Copy the next entry into this slot */
        memcpy(ctl, next, sizeof(*ctl));

        /* Free the memory we allocated for that entry */
        kfree(next);
    } else
        /* No next entry, we can just zero out this entire slot */
        memset(ctl, 0, sizeof(*ctl));
}

int device_unmap_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code)
{
    u32 idx;
    device_ctl_t* ret;

    if (!map || !code)
        return -KERR_INVAL;

    /* Create an index from this code */
    idx = __hash_device_ctlc_to_idx(map, code);

    /* Find the control code at this location */
    ret = &map->map[idx];

    /* This entry is directly inside the map, no need to free shit */
    if (ret->e_code == code) {
        __clean_ctlmap_entry(ret, ret->next);
        return 0;
    }

    /* Try to find the code in the link */
    while (ret && ret->e_code != code)
        ret = ret->next;

    if (!ret)
        return -KERR_NOT_FOUND;

    /* Clean up the entry */
    __clean_ctlmap_entry(ret, ret->next);

    /* Free the memory we needed to allocate for this slot */
    kfree(ret);
    return 0;
}

int device_ctl(device_ctlmap_t* map, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t bsize)
{
    device_ctl_t* ctl;

    ctl = __device_map_get_ctl(map, code);

    if (!ctl)
        return -KERR_NOT_FOUND;

    if (!ctl->f_impl)
        return -KERR_INVAL;

    /* Increase call cound for this fucker */
    if (ctl->s_n_calls == 0xffff) {
        ctl->b_flags |= DEVICE_CTL_FLAG_CALL_ROLLOVER;
        ctl->s_n_calls = 0;
    } else
        ctl->s_n_calls++;

    /* Call the implementation */
    return ctl->f_impl(map->p_device, ctl->p_driver, offset, buffer, bsize);
}

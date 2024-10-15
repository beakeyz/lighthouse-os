#include "system/profile/attr.h"
#include "libk/flow/error.h"
#include <libk/string.h>

int init_pattr(pattr_t* p_pattr, enum PROFILE_TYPE type, pattr_flags_t attr_flags[NR_PROFILE_TYPES])
{
    if (!p_pattr)
        return -KERR_INVAL;

    memset(p_pattr, 0, sizeof(*p_pattr));

    p_pattr->ptype = type;

    /* If there are no attribute flags specified, privilege will simply be based on
     * profile type
     */
    if (attr_flags)
        /* Copy the attribute flags */
        for (int i = 0; i < NR_PROFILE_TYPES; i++)
            p_pattr->pflags[i] = attr_flags[i];

    return 0;
}

void pattr_clear(pattr_t* attr)
{
    if (!attr)
        return;

    /* Clear all flags */
    memset(attr, 0, sizeof(*attr));

    /* Restrict all access */
    attr->ptype = PROFILE_TYPE_LIMITED;
}

void pattr_mask(pattr_t* attr, pattr_flags_t mask[NR_PROFILE_TYPES])
{
    if (!attr)
        return;

    for (int i = 0; i < NR_PROFILE_TYPES; i++)
        attr->pflags[i] &= mask[i];
}

bool pattr_hasperm(pattr_t* higher, pattr_t* lower, pattr_flags_t flags)
{
    u32 lower_flags, higher_flags;

    /* Admin always has permission */
    if (lower->ptype == PROFILE_TYPE_ADMIN)
        return true;

    /* If the 'lower' attribute set has a higher type, we always have permission */
    if (lower->ptype < higher->ptype)
        return true;

    /* Grab the target lower flags */
    lower_flags = lower->pflags[higher->ptype] & flags;
    higher_flags = higher->pflags[lower->ptype] & flags;

    /* Check if all flags are present */
    return (higher_flags == lower_flags);
}

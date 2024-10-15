#ifndef __ANIVA_PROC_PENV__
#define __ANIVA_PROC_PENV__

#include "sync/mutex.h"
#include "system/profile/attr.h"
#include <libk/stddef.h>

struct proc;
struct sysvar;
struct oss_node;
struct user_profile;

/*
 * Process environment
 *
 * Groups processes together, which share privileges, ctlvars, ect.
 */
typedef struct penv {
    const char* label;
    uint32_t flags;
    uint32_t proc_count;

    /* This envs pattrs (Inherited from parent profile) */
    pattr_t attr;

    /* The pflags mask for pattr */
    uint32_t pflags_mask[NR_PROFILE_TYPES];

    mutex_t* lock;

    struct oss_node* node;
    struct user_profile* profile;
} penv_t;

penv_t* create_penv(const char* label, uint32_t pflags_mask[NR_PROFILE_TYPES], uint32_t flags);
void destroy_penv(penv_t* env);

int penv_add_proc(penv_t* env, struct proc* p);
int penv_remove_proc(penv_t* env, struct proc* p);

#endif // !__ANIVA_PROC_PENV__

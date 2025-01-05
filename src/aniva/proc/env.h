#ifndef __ANIVA_PROC_PENV__
#define __ANIVA_PROC_PENV__

#include "sync/mutex.h"
#include "system/profile/attr.h"
#include "system/sysvar/var.h"
#include <libk/stddef.h>

struct proc;
struct sysvar;
struct oss_node;
struct user_profile;

#define PENV_VMEM_NODE_NAME "vmem"

/*
 * Process environment
 *
 * Manages the variables, permissions, ect. of a process
 */
typedef struct penv {
    const char* label;
    uint32_t flags;

    /* This envs pattrs (Inherited from parent profile) */
    pattr_t attr;

    /* The pflags mask for pattr */
    uint32_t pflags_mask[NR_PROFILE_TYPES];

    mutex_t* lock;
    /* The process of this environment */
    struct proc* p;

    struct oss_node* node;
    struct oss_node* vmem_node;
    struct user_profile* profile;
} penv_t;

penv_t* create_penv(const char* label, struct proc* p, uint32_t pflags_mask[NR_PROFILE_TYPES], uint32_t flags);
void destroy_penv(penv_t* env);

int penv_add_vector(penv_t* env, sysvar_vector_t vec);

int penv_get_var(penv_t* env, const char* key, sysvar_t** p_var);

#endif // !__ANIVA_PROC_PENV__

#include "env.h"
#include "libk/flow/error.h"
#include "lightos/sysvar/shared.h"
#include "mem/heap.h"
#include "oss/node.h"
#include "proc/proc.h"
#include "sync/mutex.h"
#include "system/profile/attr.h"
#include "system/profile/profile.h"
#include "system/sysvar/map.h"
#include "system/sysvar/var.h"
#include <libk/string.h>

penv_t* create_penv(const char* label, proc_t* p, uint32_t pflags_mask[NR_PROFILE_TYPES], uint32_t flags)
{
    penv_t* env;

    if (!label)
        return nullptr;

    env = kmalloc(sizeof(*env));

    memset(env, 0, sizeof(*env));

    env->p = p;
    env->node = create_oss_node(label, OSS_PROC_ENV_NODE, NULL, NULL);

    /* Try to add to the node */
    if (oss_node_add_obj(env->node, p->obj)) {
        destroy_oss_node(env->node);
        kfree(env);
        return nullptr;
    }

    /* Set the processes env pointer */
    p->m_env = env;

    /* Set the rest of the fields */
    env->flags = flags;
    env->label = strdup(label);
    env->lock = create_mutex(NULL);

    if (pflags_mask)
        memcpy(env->pflags_mask, pflags_mask, sizeof(*pflags_mask) * NR_PROFILE_TYPES);
    else
        /*
         * Otherwise just set all flags
         * This will make any object that enables an attribute for us visible
         */
        memset(env->pflags_mask, 0xff, sizeof(*pflags_mask) * NR_PROFILE_TYPES);

    /* Set the thing */
    env->node->priv = env;

    return env;
}

void destroy_penv(penv_t* env)
{
    destroy_mutex(env->lock);

    if (env->profile)
        profile_remove_penv(env->profile, env);

    destroy_oss_node(env->node);

    kfree((void*)env->label);
    kfree(env);
}

int penv_add_vector(penv_t* env, sysvar_vector_t vec)
{
    int error;
    u32 i = 0;

    if (!env || !vec)
        return -KERR_INVAL;

    /* Loop over all sysvar templates in the vector */
    for (struct sysvar_template* var = &vec[0]; var->key != nullptr; var = &vec[i]) {
        /* Construct a sysvar and add it to the env */
        if ((error = sysvar_attach(env->node, &var->key[1], env->profile->attr.ptype, sysvar_template_get_type(var), NULL, var->qwrd_val)))
            break;

        i++;
    }

    return error;
}

int penv_get_var(penv_t* env, const char* key, sysvar_t** p_var)
{
    if (!env || !key || !p_var)
        return -KERR_INVAL;

    /* Look for the sysvar in our node */
    *p_var = sysvar_get(env->node, key);

    /* Return the correct code */
    return (*p_var == nullptr) ? (-KERR_NOT_FOUND) : 0;
}

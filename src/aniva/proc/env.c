#include "env.h"
#include "libk/data/linkedlist.h"
#include "mem/heap.h"
#include "proc/proc.h"
#include "sync/mutex.h"
#include "system/profile/profile.h"
#include "system/sysvar/map.h"
#include "system/sysvar/var.h"
#include <libk/string.h>

static inline bool _penv_has_own_sysvar_map(penv_t* env)
{
  return (env->vars && env->vars != env->profile->vars);
}

penv_t* create_penv(const char* label, uint32_t priv_level, uint32_t flags, struct user_profile* profile)
{
  penv_t* env;

  if (!label || !profile)
    return nullptr;
  
  /* Invalid privilege level */
  if (priv_level >= PRIV_LVL_NONE)
    return nullptr;

  env = kmalloc(sizeof(*env));

  memset(env, 0, sizeof(*env));
  
  /* Grab a new ID */
  env->id = profile_add_penv(profile, env);

  if (KERR_ERR(env->id))
    goto dealloc_and_exit;

  env->flags = flags;
  env->processes = init_list();
  env->label = strdup(label);
  env->lock = create_mutex(NULL);
  env->profile = profile;
  /* Link to the variables of it's profile */
  env->vars = profile->vars;

  return env;

dealloc_and_exit:
  kfree(env);
  return nullptr;
}

void destroy_penv(penv_t* env)
{
  destroy_list(env->processes);
  destroy_mutex(env->lock);

  if (_penv_has_own_sysvar_map(env))
    destroy_sysvar_map(env->vars);

  kfree((void*)env->label);
  kfree(env);
}

int penv_add_proc(penv_t* env, proc_t* p)
{
  if (!env || !p)
    return -1;

  if (p->m_env == env)
    return 0;

  /* Remove the process from it's old environment, if it had one */
  if (p->m_env)
    penv_remove_proc(p->m_env, p);

  list_append(env->processes, p);

  p->m_env = env;
  return 0;
}

int penv_remove_proc(penv_t* env, struct proc* p)
{
  if (!env || !p)
    return -KERR_INVAL;

  if (p->m_env != env)
    return -KERR_NOT_FOUND;

  if (!list_remove_ex(env->processes, p))
    return -KERR_NOT_FOUND;

  /* No processes left, we can destroy this environment */
  if (!env->processes->m_length)
    destroy_penv(env);

  return 0;
}

/*!
 * @brief: Get a sysvar from this environment
 *
 * If the environment does not have its own varmap, the map of its profile
 * is used. Otherwise both the varmap of the environment and of the profile
 * are checked.
 */
sysvar_t* penv_get_sysvar(penv_t* env, const char* key)
{
  sysvar_t* ret;

  /* This would be weird */
  if (!env->vars)
    return nullptr;

  ret = sysvar_map_get(env->vars, key);

  if (ret)
    return ret;

  /* Return if environment is linked to it's profile */
  if (!_penv_has_own_sysvar_map(env))
    return nullptr;

  /* Look again in the profiles variable map */
  return sysvar_map_get(env->profile->vars, key);
}

/*!
 * @brief: Add a sysvar to the sysvar map of an environment
 *
 * If the environment does not yet have it's own sysvar map, it is created right here
 */
kerror_t penv_add_sysvar(penv_t* env, const char* key, enum SYSVAR_TYPE type, uint8_t flags, uintptr_t value)
{
  if (!env || !key)
    return -KERR_INVAL;

  mutex_lock(env->lock);

  /* Create a new sysvar map only once there is a variable that gets added to this penv */
  if (!_penv_has_own_sysvar_map(env))
    env->vars = create_sysvar_map(512, &env->priv_level);

  mutex_unlock(env->lock);

  return sysvar_map_put(env->vars, key, type, flags, value);
}

#include "env.h"
#include "mem/heap.h"
#include "oss/node.h"
#include "proc/proc.h"
#include "sync/mutex.h"
#include "system/profile/profile.h"
#include "system/sysvar/var.h"
#include <libk/string.h>

penv_t* create_penv(const char* label, uint32_t priv_level, uint32_t flags)
{
  penv_t* env;

  if (!label)
    return nullptr;
  
  /* Invalid privilege level */
  if (priv_level >= PRIV_LVL_NONE)
    return nullptr;

  env = kmalloc(sizeof(*env));

  memset(env, 0, sizeof(*env));

  env->flags = flags;
  env->label = strdup(label);
  env->lock = create_mutex(NULL);
  env->node = create_oss_node(label, OSS_PROC_ENV_NODE, NULL, NULL);

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

int penv_add_proc(penv_t* env, proc_t* p)
{
  int error;

  if (!env || !p)
    return -1;

  if (p->m_env == env)
    return 0;

  error = 0;

  /* Remove the process from it's old environment, if it had one */
  if (p->m_env)
    error = penv_remove_proc(p->m_env, p);

  if (error)
    return error;

  /* Add to the node */
  error = oss_node_add_obj(env->node, p->obj);

  if (KERR_ERR(error))
    return error;

  p->m_env = env;
  env->proc_count++;

  printf("Added proc (%s) to env (%s): count=%d\n", p->m_name, env->label, env->proc_count);

  return 0;
}

int penv_remove_proc(penv_t* env, struct proc* p)
{
  int error;
  oss_node_entry_t* entry;

  if (!env || !p)
    return -KERR_INVAL;

  if (p->m_env != env)
    return -KERR_NOT_FOUND;

  error = oss_node_remove_entry(env->node, p->obj->name, &entry);

  if (error)
    return -KERR_NOT_FOUND;

  printf("Removing proc (%s) from env (%s): count=%d\n", p->m_name, env->label, env->proc_count);
  /* Just to be safe, check bounds */
  if (env->proc_count)
    env->proc_count--;

  /* Empty environment, yeet it */
  if (!env->proc_count)
    destroy_penv(env);

  return 0;
}

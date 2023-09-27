#include "profile.h"
#include "libk/data/hashmap.h"
#include "libk/data/vector.h"
#include "libk/flow/error.h"
#include "proc/proc.h"
#include "proc/profile/variable.h"
#include "sync/mutex.h"
#include <libk/string.h>

#define MAX_ACTIVE_PROFILES 512

static proc_profile_t base_profile;
static proc_profile_t global_profile;
static hashmap_t* active_profiles;

static mutex_t* profile_mutex;

void init_proc_profile(proc_profile_t* profile, char* name, uint8_t level)
{
  if (!profile || !name || (level > 0x0f))
    return;

  memset(profile, 0, sizeof(proc_profile_t));

  profile->name = name;
  profile->priv_level.lvl = level;
  profile->var_map = create_hashmap(PRF_MAX_VARS, HASHMAP_FLAG_SK);
  profile->lock = create_mutex(NULL);

  profile_set_priv_check(profile);
}

void destroy_proc_profile(proc_profile_t* profile)
{
  if (!profile)
    return;

  /* Lock the mutex to prevent any async weirdness */
  mutex_lock(profile->lock);

  destroy_hashmap(profile->var_map);
  destroy_mutex(profile->lock);
}

/*!
 * @brief Register a process to the base profile
 *
 * This fails if the process is already registered.
 * NOTE: the caller should check if the process has permisson to
 * be registered to base
 */
int proc_register_to_base(proc_t* p)
{
  if (!p || p->m_profile)
    return -1;

  proc_set_profile(p, &base_profile);

  return 0;
}

/*!
 * @brief Register a process to the global profile
 *
 * This fails if the process is already registered.
 */
int proc_register_to_global(struct proc* p)
{
  if (!p || p->m_profile)
    return -1;

  proc_set_profile(p, &global_profile);

  return 0;
}

/*!
 * @brief Find the profile with a give name @name
 *
 * Nothing to add here...
 */
int profile_find(const char* name, proc_profile_t** profile)
{
  proc_profile_t* ret;

  if (!name || !profile)
    return -1;

  mutex_lock(profile_mutex);

  ret = hashmap_get(active_profiles, (void*)name);

  mutex_unlock(profile_mutex);

  if (!ret)
    return -2;

  *profile = ret;
  return 0;
}

/*!
 * @brief Add a variable to a profile
 *
 * Nothing to add here...
 */
int profile_add_var(proc_profile_t* profile, profile_var_t* var)
{
  ErrorOrPtr result;

  if (!profile || !profile->var_map || !var)
    return -1;

  mutex_lock(profile->lock);

  /* Hashmap SHOULD handle duplicates for us correctly =) */
  result = hashmap_put(profile->var_map, (void*)var->key, var);

  mutex_unlock(profile->lock);

  if (IsError(result))
    return -3;

  return 0;
}

/*!
 * @brief Remove a variable on a profile
 *
 * Nothing to add here...
 */
int profile_remove_var(proc_profile_t* profile, const char* key)
{
  ErrorOrPtr result;

  if (!profile || !profile->var_map || !key)
    return -1;

  mutex_lock(profile->lock);

  /* Hashmap SHOULD handle duplicates for us correctly =) */
  result = hashmap_remove(profile->var_map, (void*)key);

  mutex_unlock(profile->lock);

  if (IsError(result))
    return -2;

  return 0;
}

/*!
 * @brief Grab a variable on the profile by its key
 *
 * NOTE: increases the refcount of the variable on success, which 
 * the caller needs to release
 */
int profile_get_var(proc_profile_t* profile, const char* key, profile_var_t** var)
{
  profile_var_t* ret;

  if (!profile || !profile->var_map || !var)
    return -1;

  mutex_lock(profile->lock);

  ret = hashmap_get(profile->var_map, (void*)key);

  mutex_unlock(profile->lock);

  if (!ret)
    return -2;

  /* Increase the refcount of the variable */
  *var = get_profile_var(ret); 

  return 0;
}

int profile_register(proc_profile_t* profile)
{
  ErrorOrPtr result;

  if (!profile)
    return -1;
  
  mutex_lock(profile_mutex);

  result = hashmap_put(active_profiles, (void*)profile->name, profile);

  mutex_unlock(profile_mutex);

  if (IsError(result))
    return -2;

  return 0;
}

int profile_unregister(const char* name)
{
  ErrorOrPtr result;

  if (!name || !name[0])
    return -1;
  
  mutex_lock(profile_mutex);

  result = hashmap_remove(active_profiles, (void*)name);

  mutex_unlock(profile_mutex);

  if (IsError(result))
    return -2;

  return 0;
}

/*!
 * @brief Grab the amount of active profiles
 *
 * Nothing to add here...
 */
uint16_t get_active_profile_count()
{
  return active_profiles->m_size;
}

/*!
 * @brief Initialise the standard profiles present on all systems
 *
 * We initialize the profile BASE for any kernel processes and 
 * Global for any basic global processes. When a user signs up, they
 * create a profile they get to name (Can't be duplicate)
 */
void init_proc_profiles(void)
{
  profile_mutex = create_mutex(NULL);

  init_proc_profile(&base_profile, "BASE", PRF_PRIV_LVL_BASE);
  init_proc_profile(&global_profile, "Global", PRF_PRIV_LVL_USER);

  active_profiles = create_hashmap(MAX_ACTIVE_PROFILES, HASHMAP_FLAG_SK);

  hashmap_put(active_profiles, (void*)base_profile.name, &base_profile);
  hashmap_put(active_profiles, (void*)global_profile.name, &global_profile);

  profile_var_t* var = create_profile_var("SYSTEM_NAME", PROFILE_VAR_TYPE_STRING, "Aniva");

  profile_add_var(&base_profile, var);
}

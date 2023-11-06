#include "variable.h"
#include "LibSys/proc/var_types.h"
#include "mem/heap.h"
#include "mem/zalloc.h"
#include "proc/profile/profile.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"

static zone_allocator_t __var_allocator;

static uint32_t profile_var_get_size_for_type(profile_var_t* var) 
{
  if (!var)
    return 0;

  switch (var->type) {
    case PROFILE_VAR_TYPE_BYTE:
      return sizeof(uint8_t);
    case PROFILE_VAR_TYPE_WORD:
      return sizeof(uint16_t);
    case PROFILE_VAR_TYPE_DWORD:
      return sizeof(uint32_t);
    case PROFILE_VAR_TYPE_QWORD:
      return sizeof(uint64_t);
    case PROFILE_VAR_TYPE_STRING:
      if (!var->str_value)
        return 0;

      return strlen(var->str_value) + 1;
  }
  return 0;
} 

static void destroy_profile_var(profile_var_t* var)
{
  if (var->profile) {
    
    if (profile_is_from_file(var->profile)) {
      kfree((void*)var->key);

      /* Just in case this was allocated on the heap (by strdup) */
      if (var->type == PROFILE_VAR_TYPE_STRING && var->str_value)
        kfree((void*)var->str_value);
    }

    profile_remove_var(var->profile, var->key);
  }
  
  destroy_atomic_ptr(var->refc);
  zfree_fixed(&__var_allocator, var);
}

/*!
 * @brief: Creates a profile variable
 *
 * This allocates the variable inside the variable zone allocator and sets its interlan value
 * equal to @value. 
 * NOTE: this does not copy strings and allocate them on the heap!
 */
profile_var_t* create_profile_var(const char* key, enum PROFILE_VAR_TYPE type, uint8_t flags, uintptr_t value)
{
  profile_var_t* var;

  var = zalloc_fixed(&__var_allocator); 

  if (!var)
    return nullptr;

  memset(var, 0, sizeof *var);

  var->key = key;
  var->type = type;
  var->flags = flags;

  /* Placeholder value set */
  var->value = (void*)value;

  /*
   * Set the refcount to one in order to preserve the variable for multiple gets
   * and releases
   */
  var->refc = create_atomic_ptr_with_value(1);
  /* Make sure type and value are already set */
  var->len = profile_var_get_size_for_type(var);

  return var;
}

profile_var_t* get_profile_var(profile_var_t* var)
{
  if (!var)
    return nullptr;

  uint64_t current_count = atomic_ptr_load(var->refc);
  atomic_ptr_write(var->refc, current_count+1);

  return var;
}

void release_profile_var(profile_var_t* var)
{
  uint64_t current_count = atomic_ptr_load(var->refc);

  if (current_count)
    atomic_ptr_write(var->refc, current_count-1);

  if (!current_count)
    destroy_profile_var(var);
}

bool profile_var_get_str_value(profile_var_t* var, const char** buffer)
{
  if (!buffer || !var || var->type != PROFILE_VAR_TYPE_STRING)
    return false;

  if (!var->profile)
    return false;

  mutex_lock(var->profile->lock);

  *buffer = var->str_value;

  mutex_unlock(var->profile->lock);

  return true;
}

bool profile_var_get_qword_value(profile_var_t* var, uint64_t* buffer)
{
  if (!buffer || !var || var->type != PROFILE_VAR_TYPE_QWORD)
    return false;

  if (!var->profile)
    return false;

  mutex_lock(var->profile->lock);

  *buffer = var->qword_value;

  mutex_unlock(var->profile->lock);

  return true;
}

bool profile_var_get_dword_value(profile_var_t* var, uint32_t* buffer)
{
  if (!buffer || !var || var->type != PROFILE_VAR_TYPE_DWORD)
    return false;

  if (!var->profile)
    return false;

  mutex_lock(var->profile->lock);

  *buffer = var->dword_value;

  mutex_unlock(var->profile->lock);

  return true;
}

bool profile_var_get_word_value(profile_var_t* var, uint16_t* buffer)
{
  if (!buffer || !var || var->type != PROFILE_VAR_TYPE_WORD)
    return false;

  if (!var->profile)
    return false;

  mutex_lock(var->profile->lock);

  *buffer = var->word_value;

  mutex_unlock(var->profile->lock);

  return true;
}

bool profile_var_get_byte_value(profile_var_t* var, uint8_t* buffer)
{
  if (!buffer || !var || var->type != PROFILE_VAR_TYPE_BYTE)
    return false;

  if (!var->profile)
    return false;

  mutex_lock(var->profile->lock);

  *buffer = var->byte_value;

  mutex_unlock(var->profile->lock);

  return true;
}

/*!
 * @brief Write to the variables internal value
 *
 * NOTE: when dealing with strings in profile variables, we simply store a pointer
 * to a array of characters, but we never opperate on the string itself. This is left
 * to the caller. This means that when @var holds a pointer to a malloced string or 
 * anything like that, the caller MUST free it before changing the profile var value,
 * otherwise this malloced string is lost to the void =/
 */
bool profile_var_write(profile_var_t* var, uint64_t value)
{
  if (!var || !var->profile)
    return false;

  /* Yikes */
  if ((var->flags & PVAR_FLAG_CONSTANT) == PVAR_FLAG_CONSTANT)
    return false;

  mutex_lock(var->profile->lock);

  switch (var->type) {
    case PROFILE_VAR_TYPE_STRING:
      var->str_value = (const char*)value;
      break;
    case PROFILE_VAR_TYPE_QWORD:
      var->qword_value = value;
      break;
    case PROFILE_VAR_TYPE_DWORD:
      var->dword_value = (uint32_t)value;
      break;
    case PROFILE_VAR_TYPE_WORD:
      var->word_value = (uint16_t)value;
      break;
    case PROFILE_VAR_TYPE_BYTE:
      var->byte_value = (uint8_t)value;
      break;
  }

  mutex_unlock(var->profile->lock);

  return true;
}

void init_proc_variables(void)
{
  Must(init_zone_allocator(&__var_allocator, (16 * Kib), sizeof(profile_var_t), NULL));
}

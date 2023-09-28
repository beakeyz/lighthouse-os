#include "variable.h"
#include "mem/zalloc.h"
#include "proc/profile/profile.h"
#include "sync/atomic_ptr.h"

static zone_allocator_t __var_allocator;

static void destroy_profile_var(profile_var_t* var)
{
  if (var->profile)
    profile_remove_var(var->profile, var->key);
  
  destroy_atomic_ptr(var->refc);
  zfree_fixed(&__var_allocator, var);
}

profile_var_t* create_profile_var(const char* key, enum PROFILE_VAR_TYPE type, void* value)
{
  profile_var_t* var;

  var = zalloc_fixed(&__var_allocator); 

  if (!var)
    return nullptr;

  memset(var, 0, sizeof *var);

  var->key = key;
  var->type = type;

  /* Placeholder value set */
  var->qword_value = (uint64_t)value;

  /*
   * Set the refcount to one in order to preserve the variable for multiple gets
   * and releases
   */
  var->refc = create_atomic_ptr_with_value(1);

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

  *buffer = var->str_value;

  return true;
}

bool profile_var_get_qword_value(profile_var_t* var, uint64_t* buffer)
{
  if (!buffer || !var || var->type != PROFILE_VAR_TYPE_QWORD)
    return false;

  *buffer = var->qword_value;

  return true;
}

bool profile_var_get_dword_value(profile_var_t* var, uint32_t* buffer)
{
  if (!buffer || !var || var->type != PROFILE_VAR_TYPE_DWORD)
    return false;

  *buffer = var->dword_value;

  return true;
}

bool profile_var_get_word_value(profile_var_t* var, uint16_t* buffer)
{
  if (!buffer || !var || var->type != PROFILE_VAR_TYPE_WORD)
    return false;

  *buffer = var->word_value;

  return true;
}

bool profile_var_get_byte_value(profile_var_t* var, uint8_t* buffer)
{
  if (!buffer || !var || var->type != PROFILE_VAR_TYPE_BYTE)
    return false;

  *buffer = var->byte_value;

  return true;
}

void init_proc_variables(void)
{
  Must(init_zone_allocator(&__var_allocator, (16 * Kib), sizeof(profile_var_t), NULL));
}

#include "profile.h"
#include "LibSys/proc/var_types.h"
#include "entry/entry.h"
#include "libk/data/hashmap.h"
#include "libk/data/vector.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "proc/proc.h"
#include "proc/profile/variable.h"
#include "sync/mutex.h"
#include <libk/string.h>

#define MAX_ACTIVE_PROFILES 512

/* The variable key that holds the password */
#define PROFILE_PASSWORD_KEY "PASSWORD"

static proc_profile_t base_profile;
static proc_profile_t global_profile;
static hashmap_t* active_profiles;

static mutex_t* profile_mutex;

void init_proc_profile(proc_profile_t* profile, char* name, uint8_t level)
{
  if (!profile || !name || (level > PRF_PRIV_LVL_USER))
    return;

  memset(profile, 0, sizeof(proc_profile_t));

  profile->name = name;
  profile->var_map = create_hashmap(PRF_MAX_VARS, HASHMAP_FLAG_SK);
  profile->lock = create_mutex(NULL);

  profile_set_priv_lvl(profile, level);
}

void destroy_proc_profile(proc_profile_t* profile)
{
  if (!profile)
    return;

  /* Lock the mutex to prevent any async weirdness */
  mutex_lock(profile->lock);

  /* When we loaded a profile from a file, we malloced these strings */
  if (profile_is_from_file(profile)) {
    kfree((void*)profile->name);
    kfree((void*)profile->path);
  }

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

  if (!var || !profile || !profile->var_map)
    return -1;

  mutex_lock(profile->lock);

  /* Hashmap SHOULD handle duplicates for us correctly =) */
  result = hashmap_put(profile->var_map, (void*)var->key, var);

  mutex_unlock(profile->lock);

  if (IsError(result))
    return -3;

  var->profile = profile;
  return 0;
}

/*!
 * @brief Remove a variable on a profile
 *
 * This leaves the ->profile field on the removed profile_var set
 * and the caller should handle this
 */
int profile_remove_var(proc_profile_t* profile, const char* key)
{
  ErrorOrPtr result;

  if (!key || !profile || !profile->var_map)
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

  if (!var || !profile || !profile->var_map)
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

/*!
 * @brief Find a profile-variable pair based on one unified path
 *
 * The path is constructed as followed: [profile_name]/[var_name]
 * If the path does not contain a slash, we will immediately exit the function
 */
int profile_scan_var(const char* path, proc_profile_t** profile, profile_var_t** var)
{
  int error;
  size_t path_len;
  char* profile_name;
  char* var_name;
  proc_profile_t* profile_ptr;
  profile_var_t* var_ptr;

  if (!profile || !var)
    return -1;

  path_len = strlen(path) + 1;
  char path_buffer[path_len];

  /* Copy everything WITH the null-byte */
  memcpy(path_buffer, path, path_len);

  var_name = profile_name = &path_buffer[0];

  while (*var_name != '/')
    var_name++;

  /* No slash found =/ */
  if (!(*var_name))
    return -2;

  *var_name = '\0';
  var_name++;

  /* No var name? seriously? */
  if (!(*var_name))
    return -3;

  profile_ptr = NULL;
  var_ptr = NULL;
  error = profile_find(profile_name, &profile_ptr);

  /* Copy the profile over after the search */
  *profile = profile_ptr;

  if (error || !profile_ptr)
    return -4;

  error = profile_get_var(profile_ptr, var_name, &var_ptr);

  /* Copy the var over after the search */
  *var = var_ptr;

  if (error || !var_ptr)
    return -5;

  return 0;
}

bool profile_can_see_var(proc_profile_t* profile, profile_var_t* var)
{
  if ((var->flags & PVAR_FLAG_HIDDEN) == PVAR_FLAG_HIDDEN)
    return false;

  /* Same profile address, same profile */
  if (profile == var->profile)
    return true;

  return (profile_get_priv_lvl(profile) <= profile_get_priv_lvl(var->profile));
}

/*!
 * @brief Register a profile to the active list
 *
 */
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

/*!
 * @brief Remove a profile from the active list
 *
 * Nothing to add here...
 */
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
 * @brief: Try to set a password variable in a profile
 *
 * Allocates a direct copy of the password on the kernel heap. It is vital 
 * that userspace is not able to directly access this thing through some exploit,
 * since that would be fucky lmao
 * 
 * @returns: 0 when we could set a password on this profile
 */
int profile_set_password(proc_profile_t* profile, const char* password)
{
  int error;
  char* password_cpy;
  profile_var_t* password_var;

  if (!profile)
    return -1;

  mutex_lock(profile->lock);
  
  error = NULL;
  password_var = nullptr;
  password_cpy = strdup(password);

  /* Do we already have a password variable on this boi? */
  if (profile_has_password(profile)) {
    error = profile_get_var(profile, PROFILE_PASSWORD_KEY, &password_var);

    /* Invalid variable on this one */
    if (error || !password_var || password_var->type != PROFILE_VAR_TYPE_STRING)
      goto fail_and_exit;

    /* Free the value that was previously on this variable */
    if (password_var->value)
      kfree((void*)password_var->value);

    /* Make sure this variable is hidden */
    password_var->flags |= PVAR_FLAG_HIDDEN;
    password_var->value = password_cpy;

    goto unlock_and_exit;
  }

  /* Create a new variable, since this profile didn't have one yet */
  password_var = create_profile_var(PROFILE_PASSWORD_KEY, PROFILE_VAR_TYPE_STRING, PVAR_FLAG_HIDDEN, (uint64_t)password_cpy);

  if (!password_var) {
    error = -1;
    goto fail_and_exit;
  }

  error = profile_add_var(profile, password_var);

  /* If we succeeded with adding this profile variable, skip the fail label and unlock the mutex */
  if (!error)
    goto unlock_and_exit;

fail_and_exit:
  if (password_cpy)
    kfree((void*)password_cpy);

unlock_and_exit:
  mutex_unlock(profile->lock);
  return error;
}

/*!
 * @brief: Tries to remove the password variable from the profile
 *
 * Nothing to add here...
 */
int profile_clear_password(proc_profile_t* profile)
{
  if (!profile_has_password(profile))
    return -1;

  return profile_remove_var(profile, PROFILE_PASSWORD_KEY);
}

/*!
 * @brief: Computes the length of the current password of a profile
 *
 * Fails if the profile does not contain a valid profile variable
 */
int profile_get_password_len(proc_profile_t* profile, size_t* size)
{
  int error;
  profile_var_t* var;

  if (!size)
    return -1;
  
  *size = 0;

  if (!profile_has_valid_password_var(profile))
    return -1;

  error = profile_get_var(profile, PROFILE_PASSWORD_KEY, &var);

  if (error || !var)
    return -2;

  *size = strlen((char*)var->value) + 1;

  return 0;
}

/*!
 * @brief: Copies the profiles password into @buffer
 * 
 * NOTE: The caller is responsible to make sure that the buffer is the correct size
 *
 * @returns: A negative integer on failure, The difference in length between the password and
 * the buffer on success (positive or zero)
 */
int profile_get_password(proc_profile_t* profile, uint8_t* buffer, size_t size)
{
  int error;
  size_t password_len;
  profile_var_t* var;

  if (!buffer || !size)
    return -1;

  if (!profile_has_valid_password_var(profile))
    return -1;

  error = profile_get_var(profile, PROFILE_PASSWORD_KEY, &var);

  if (error || !var)
    return -2;

  password_len = strlen(var->value);

  /* Copy the password */
  memcpy(buffer, var->value, size);

  if (size > password_len)
    return (size - password_len);

  return (password_len - size);
}

/*!
 * @brief: Checks if @profile contains a password variable
 *
 * NOTE: this variable should really be hidden, but this function does not check
 * for the validity of the variable
 */
bool profile_has_password(proc_profile_t* profile)
{
  int error;
  profile_var_t* pswrd_var;

  if (!profile)
    return false;

  pswrd_var = nullptr;
  error = profile_get_var(profile, PROFILE_PASSWORD_KEY, &pswrd_var);

  if (error || !pswrd_var)
    return false;

  return true;
}

/*!
 * @brief: Checks if @profile has a valid password variable
 *
 * A password variable is valid if its hidden and of the string type
 */
bool profile_has_valid_password_var(proc_profile_t* profile)
{
  int error;
  profile_var_t* var;

  if (!profile_has_password(profile))
    return false;

  error = profile_get_var(profile, PROFILE_PASSWORD_KEY, &var);

  /* This should not fail if profile has a password lmao */
  if (error)
    return false;

  return (var->value && var->type == PROFILE_VAR_TYPE_STRING && (var->flags & PVAR_FLAG_HIDDEN) == PVAR_FLAG_HIDDEN);
}

#define LT_PFB_MAGIC0 'P'
#define LT_PFB_MAGIC1 'F'
#define LT_PFB_MAGIC2 'b'
#define LT_PFB_MAGIC3 '\e'
#define LT_PFB_MAGIC "PFb\e"

/*
 * Structure of the file that contains a profile
 * this buffer starts at offset 0 inside the file
 *
 * When saving/loading this struct to/from a file, notice that we are making use of a
 * string table, just like in the ELF file format. This means that any
 * char* that we find in the file, are actually offsets into the string-
 * table
 */
struct lt_profile_buffer {
  char magic[4];
  /* Version of the kernel this was made for */
  uint32_t kernel_version;
  /* Offset from the start of the file to the start of the string table */
  uint32_t strtab_offset;
  proc_profile_t profile;
  profile_var_t vars[];
} __attribute__((packed));

/*!
 * @brief Save a profile to a file
 *
 * Nothing to add here...
 */
int profile_save(proc_profile_t* profile, file_t* file)
{
  kernel_panic("TODO: profile_save");
  return 0;
}

int profile_load(proc_profile_t** profile, file_t* file)
{
  kernel_panic("TODO: profile_load");
  return 0;
}

/*!
 * @brief: Saves variables from a profile into a file 
 */
int profile_save_variables(proc_profile_t* profile, file_t* file)
{  
  kernel_panic("TODO: profile_save_variables");
  return 0;
}

/*!
 * @brief: Loads variables from a file into a profile
 */
int profile_load_variables(proc_profile_t* profile, file_t* file)
{
  kernel_panic("TODO: profile_load_variables");
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
 * @brief: Fill the BASE profile with default variables
 *
 * The BASE profile should store things that are relevant to system processes and contains
 * most of the variables that are not visible to userspace, like passwords, driver locations,
 * or general system file locations
 *
 * Little TODO, but still: The bootloader can also be aware of the files in the ramdisk when installing
 * the system to a harddisk partition. When it does, it will put drivers into their correct system directories 
 * and it can then create entries in the default BASE profile file (which we still need to load)
 */
static void __apply_base_variables()
{
  /* TODO: should we store the (encrypted) password of the base profile here? */
  //profile_add_var(&base_profile, create_profile_var("PASSWRD", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_HIDDEN, NULL));
}

/*!
 * @brief: Fill the Global profile with default variables
 *
 * These variables tell userspace about:
 * - the kernel its running on
 * - where it can find certain system files
 * - information about firmware
 * - information about devices (?)
 */
static void __apply_global_variables()
{
  profile_add_var(&global_profile, create_profile_var("SYSTEM_NAME", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_CONSTANT | PVAR_FLAG_GLOBAL, (uintptr_t)"Aniva"));
  profile_add_var(&global_profile, create_profile_var("VERSION_MAJ", PROFILE_VAR_TYPE_BYTE, PVAR_FLAG_CONSTANT | PVAR_FLAG_GLOBAL, kernel_version.maj));
  profile_add_var(&global_profile, create_profile_var("VERSION_MIN", PROFILE_VAR_TYPE_BYTE, PVAR_FLAG_CONSTANT | PVAR_FLAG_GLOBAL, kernel_version.min));
  profile_add_var(&global_profile, create_profile_var("VERSION_BUMP", PROFILE_VAR_TYPE_WORD, PVAR_FLAG_CONSTANT | PVAR_FLAG_GLOBAL, kernel_version.bump));
  //profile_add_var(&global_profile, create_profile_var("", enum PROFILE_VAR_TYPE type, uintptr_t value))
}

/*!
 * @brief Initialise the standard profiles present on all systems
 *
 * We initialize the profile BASE for any kernel processes and 
 * Global for any basic global processes. When a user signs up, they
 * create a profile they get to name (Can't be duplicate)
 *
 * We need to prevent that this system becomes like the windows registry, since
 * that's a massive dumpsterfire. We want this to kind of act like environment
 * variables but more defined, with more responsibility and volatility.
 *
 * They can be used to store system data or search paths, like in the windows registry,
 * but they should not get bloated by apps and highly suprevised by the kernel
 */
void init_proc_profiles(void)
{
  profile_mutex = create_mutex(NULL);

  init_proc_profile(&base_profile, "BASE", PRF_PRIV_LVL_BASE);
  init_proc_profile(&global_profile, "Global", PRF_PRIV_LVL_USER);

  active_profiles = create_hashmap(MAX_ACTIVE_PROFILES, HASHMAP_FLAG_SK);

  hashmap_put(active_profiles, (void*)base_profile.name, &base_profile);
  hashmap_put(active_profiles, (void*)global_profile.name, &global_profile);

  /* Apply the default variables onto the default profiles */
  __apply_base_variables();
  __apply_global_variables();
}

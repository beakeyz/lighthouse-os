#include "profile.h"
#include "crypto/k_crc32.h"
#include "fs/file.h"
#include "lightos/proc/var_types.h"
#include "entry/entry.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "proc/proc.h"
#include "proc/profile/variable.h"
#include "sync/mutex.h"
#include <libk/string.h>
#include <kevent/event.h>

#define SOFTMAX_ACTIVE_PROFILES 4096
#define DEFAULT_GLOBAL_PVR_PATH "Root/Global/global.pvr"

static proc_profile_t base_profile;
static proc_profile_t global_profile;
static hashmap_t* active_profiles;

static mutex_t* profile_mutex;

/*!
 * @brief: Initialize memory of a profile
 *
 * A single profile can store information about it's permissions and the variables 
 * that are bound to this profile. These variables are stored in a variable sized 
 * hashmap, that is not really optimized at all...
 */
int init_proc_profile(proc_profile_t* profile, proc_profile_t* parent, char* name, uint8_t level)
{
  if (!profile || !name || (level > PRF_PRIV_LVL_BASE))
    return -1;

  memset(profile, 0, sizeof(proc_profile_t));

  profile->parent = parent;
  profile->name = name;
  profile->var_map = create_hashmap(PRF_MAX_VARS, HASHMAP_FLAG_SK);
  profile->lock = create_mutex(NULL);

  profile_set_priv_lvl(profile, level);

  return 0;
}

proc_profile_t* create_proc_profile(proc_profile_t* parent, char* name, uint8_t level)
{
  proc_profile_t* ret;

  ret = kmalloc(sizeof(*ret));

  if (init_proc_profile(ret, parent, name, level)) {
    kfree(ret);
    return nullptr;
  }

  return ret;
}

/*!
 * @brief: Destroy all the memory of a profile
 *
 * FIXME: what do we do with all the variables of this profile
 */
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

  /* Profile might be allocated on the heap */
  kfree(profile);
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
 * @brief: Loop over every profile and call @fn on them
 *
 * Simply a wrapper around hashmap_itterate, so it's convention for a hashmap_itterate_fn_t
 * should be used
 */
int profile_foreach(hashmap_itterate_fn_t fn)
{
  if (!fn)
    return -1;

  /* Simply call hashmap_itterate */
  if (IsError(hashmap_itterate(active_profiles, fn, NULL, NULL)))
    return -2;

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
 * This clears the ->profile field on the removed profile_var
 * and the caller should be thoughtful of this
 */
int profile_remove_var(proc_profile_t* profile, const char* key)
{
  profile_var_t* var;

  if (!key || !profile || !profile->var_map)
    return -1;

  mutex_lock(profile->lock);

  /* NOTE: remove returns the value it removed and null on error */
  var = hashmap_remove(profile->var_map, (void*)key);

  /* Can't find this variable, yikes */
  if (!var)
    goto unlock;

  /* Make sure this field is cleared */
  var->profile = nullptr;

unlock:
  mutex_unlock(profile->lock);

  if (!var)
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
  if (profile)
    *profile = profile_ptr;

  if (error || !profile_ptr)
    return -4;

  error = profile_get_var(profile_ptr, var_name, &var_ptr);

  /* Copy the var over after the search */
  if (var)
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

  /*
   * When the priv_lvl of @profile is higher than that of the profile of @var, this
   * means that @profile 'supervises' the variables profile. This gives it access to
   * it's variables
   */
  return (profile_get_priv_lvl(profile) >= profile_get_priv_lvl(var->profile));
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
  proc_profile_t* result;

  if (!name || !name[0])
    return -1;
  
  mutex_lock(profile_mutex);

  result = hashmap_remove(active_profiles, (void*)name);

  mutex_unlock(profile_mutex);

  if (!result)
    return -2;

  return 0;
}

static int _generate_passwd_hash(const char* passwd, uint64_t* buffer)
{
  uint32_t crc;

  if (!passwd || !buffer)
    return -1;

  crc = kcrc32((uint8_t*)passwd, strlen(passwd));

  /* Set the thing lmao */
  buffer[0] = crc;
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

  error = _generate_passwd_hash(password, profile->passwd_hash);

  if (error)
    return error;

  profile->flags |= PROFILE_FLAG_HAS_PASSWD;
  return  0;
}

/*!
 * @brief: Tries to remove the password variable from the profile
 *
 * Nothing to add here...
 */
int profile_clear_password(proc_profile_t* profile)
{
  memset(profile->passwd_hash, 0, sizeof(profile->passwd_hash));
  profile->flags &= ~PROFILE_FLAG_HAS_PASSWD;
  return 0;
}

int profile_match_password(proc_profile_t* profile, const char* match)
{
  int error;
  uint64_t match_hash;

  error = _generate_passwd_hash(match, &match_hash);

  if (error)
    return error;

  if (profile->passwd_hash[0] == match_hash)
    return 0;

  return -1;
}

/*!
 * @brief: Checks if @profile contains a password variable
 *
 * NOTE: this variable should really be hidden, but this function does not check
 * for the validity of the variable
 */
bool profile_has_password(proc_profile_t* profile)
{
  return (profile->flags & PROFILE_FLAG_HAS_PASSWD) == PROFILE_FLAG_HAS_PASSWD;
}

/*!
 * @brief Grab the amount of active profiles
 *
 * Nothing to add here...
 */
uint64_t get_active_profile_count()
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
  profile_add_var(&base_profile, create_profile_var("KTERM_LOC", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_VOLATILE, PROFILE_STR("Root/System/kterm.drv")));
  profile_add_var(&base_profile, create_profile_var(DRIVERS_LOC_VARKEY, PROFILE_VAR_TYPE_STRING, PVAR_FLAG_VOLATILE, PROFILE_STR("Root/System/")));

  /* Core drivers which perform high level scanning and load low level drivers */
  profile_add_var(&base_profile, create_profile_var("USB_CORE_DRV", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_VOLATILE, PROFILE_STR("usbcore.drv")));
  profile_add_var(&base_profile, create_profile_var("DISK_CORE_DRV", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_VOLATILE, PROFILE_STR("diskcore.drv")));
  profile_add_var(&base_profile, create_profile_var("INPUT_CORE_DRV", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_VOLATILE, PROFILE_STR("inptcore.drv")));
  profile_add_var(&base_profile, create_profile_var("VIDEO_CORE_DRV", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_VOLATILE, PROFILE_STR("vidcore.drv")));
  profile_add_var(&base_profile, create_profile_var("ACPI_CORE_DRV", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_VOLATILE, PROFILE_STR("acpicore.drv")));

  /* 
   * These variables are to store the boot parameters. When we've initialized all devices we need and we're
   * ready to load our bootdevice, we will look in "BASE/BOOT_DEVICE" to find the path to our bootdevice, after 
   * which we will verify that it has remained unchanged by checking if "BASE/BOOT_DEVICE_NAME" and "BASE/BOOT_DEVICE_SIZE"
   * are still valid.
   */
  profile_add_var(&base_profile, create_profile_var(BOOT_DEVICE_VARKEY, PROFILE_VAR_TYPE_STRING, PVAR_FLAG_VOLATILE, PROFILE_STR(NULL)));
  profile_add_var(&base_profile, create_profile_var(BOOT_DEVICE_SIZE_VARKEY, PROFILE_VAR_TYPE_QWORD, PVAR_FLAG_VOLATILE, NULL));
  profile_add_var(&base_profile, create_profile_var(BOOT_DEVICE_NAME_VARKEY, PROFILE_VAR_TYPE_STRING, PVAR_FLAG_VOLATILE, PROFILE_STR(NULL)));

}

/*!
 * @brief: Fill the Global profile with default variables
 *
 * These variables tell userspace about:
 * - the kernel its running on
 * - where it can find certain system files
 * - information about firmware
 * - information about devices (?)
 * - where default events are located
 * - where utility drivers are located
 */
static void __apply_global_variables()
{
  /* System name: Codename of the kernel that it present in the system */
  profile_add_var(&global_profile, create_profile_var("SYSTEM_NAME", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_CONSTANT | PVAR_FLAG_GLOBAL, PROFILE_STR("Aniva")));
  /* Major version number of the kernel */
  profile_add_var(&global_profile, create_profile_var("VERSION_MAJ", PROFILE_VAR_TYPE_BYTE, PVAR_FLAG_CONSTANT | PVAR_FLAG_GLOBAL, kernel_version.maj));
  /* Minor version number of the kernel */
  profile_add_var(&global_profile, create_profile_var("VERSION_MIN", PROFILE_VAR_TYPE_BYTE, PVAR_FLAG_CONSTANT | PVAR_FLAG_GLOBAL, kernel_version.min));
  /* Bump version number of the kernel. This is used to indicate small changes in between different builds of the kernel */
  profile_add_var(&global_profile, create_profile_var("VERSION_BUMP", PROFILE_VAR_TYPE_WORD, PVAR_FLAG_CONSTANT | PVAR_FLAG_GLOBAL, kernel_version.bump));

  /* Default provider for lwnd services */
  profile_add_var(&global_profile, create_profile_var("DFLT_LWND_PATH", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_GLOBAL, PROFILE_STR("service/lwnd")));
  /* Default eventname for the keyboard event */
  profile_add_var(&global_profile, create_profile_var("DFLT_KB_EVENT", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_GLOBAL, PROFILE_STR("keyboard")));
  /* Path variable to indicate default locations for executables */
  profile_add_var(&global_profile, create_profile_var("PATH", PROFILE_VAR_TYPE_STRING, PVAR_FLAG_GLOBAL, PROFILE_STR("Root/Apps:Root/User/Global/Apps")));
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
 *
 * NOTE: this is called before we've completely set up our vfs structure, so we can't yet
 * load default values from any files. This is done in init_profiles_late
 */
void init_proc_profiles(void)
{
  profile_mutex = create_mutex(NULL);

  init_proc_profile(&base_profile, NULL, "BASE", PRF_PRIV_LVL_BASE);
  init_proc_profile(&global_profile, NULL, "Global", PRF_PRIV_LVL_USER);

  active_profiles = create_hashmap(SOFTMAX_ACTIVE_PROFILES, HASHMAP_FLAG_SK);

  hashmap_put(active_profiles, (void*)base_profile.name, &base_profile);
  hashmap_put(active_profiles, (void*)global_profile.name, &global_profile);

  /* Apply the default variables onto the default profiles */
  __apply_base_variables();
  __apply_global_variables();
}

/*!
 * @brief: Save current variables of the default profiles to disk
 *
 * If the files that contain the variables have not been created yet, we create them on the spot
 * There is a possibility that the system got booted with a ramdisk as a rootfs. In this case we can't
 * save any variables, since the system is in a read-only state. In order to save the variables, we'll
 * need to dynamically remount a filesystem with write capabilities.
 */
static int save_default_profiles(kevent_ctx_t* ctx)
{
  file_t* global_prf_save_file;
  (void)ctx;

  /* Try to open the default file for the Global profile */
  global_prf_save_file = file_open(global_profile.path);

  /* Shit, TODO: create this shit */
  if (!global_prf_save_file)
    return 0;

  profile_save_variables(&global_profile, global_prf_save_file);

  file_close(global_prf_save_file);
  return 0;
}

/*!
 * @brief: Late initialization of the profiles
 *
 * This keeps itselfs busy with any profile variables that where saved on disk. It also creates a shutdown eventhook
 * to save it's current default profiles to disk
 */
void init_profiles_late(void)
{
  int error = -1;
  file_t* f = file_open(DEFAULT_GLOBAL_PVR_PATH);

  if (f)
    error = profile_load_variables(&global_profile, f);

  file_close(f);

  if (!error)
    global_profile.path = DEFAULT_GLOBAL_PVR_PATH;

  /* Create an eventhook on shutdown */
  kevent_add_hook("shutdown", "save default profiles", save_default_profiles);

  /* TMP: TODO remove */
  profile_set_password(&base_profile, "admin");
}

#include "profile.h"
#include "crypto/k_crc32.h"
#include "entry/entry.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "lightos/proc/var_types.h"
#include "mem/heap.h"
#include <system/sysvar/var.h>
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "proc/env.h"
#include "sync/mutex.h"
#include "system/sysvar/loader.h"
#include "system/sysvar/map.h"
#include <libk/string.h>
#include <kevent/event.h>

#define SOFTMAX_ACTIVE_PROFILES 4096
#define DEFAULT_GLOBAL_PVR_PATH "Root/User/global.pvr"

static user_profile_t _admin_profile;
static user_profile_t _user_profile;
static oss_node_t* _runtime_node;

static mutex_t* profile_mutex;

/*!
 * @brief: Initialize memory of a profile
 *
 * A single profile can store information about it's permissions and the variables 
 * that are bound to this profile. These variables are stored in a variable sized 
 * hashmap, that is not really optimized at all...
 */
int init_proc_profile(user_profile_t* profile, char* name, uint32_t level)
{
  if (!profile || !name || (level >= PRIV_LVL_NONE))
    return -1;

  memset(profile, 0, sizeof(user_profile_t));

  profile->name = name;
  profile->lock = create_mutex(NULL);
  profile->node = create_oss_node(name, OSS_PROFILE_NODE, NULL, NULL);

  /* FIXME: Error handle */
  (void)oss_attach_node("Runtime", profile->node);

  /* Set the private field */
  profile->node->priv = profile;

  profile_set_priv_lvl(profile, level);

  return 0;
}

user_profile_t* create_proc_profile(char* name, uint8_t level)
{
  user_profile_t* ret;

  ret = kmalloc(sizeof(*ret));

  if (init_proc_profile(ret, name, level)) {
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
void destroy_proc_profile(user_profile_t* profile)
{
  if (!profile)
    return;

  /* Lock the mutex to prevent any async weirdness */
  mutex_lock(profile->lock);

  /* First off, murder the node */
  destroy_oss_node(profile->node);

  /* When we loaded a profile from a file, we malloced these strings */
  if (profile_is_from_file(profile)) {
    kfree((void*)profile->name);
    kfree((void*)profile->path);
  }

  destroy_mutex(profile->lock);

  /* Profile might be allocated on the heap */
  kfree(profile);
}

user_profile_t* get_user_profile()
{
  return &_user_profile;
}

user_profile_t* get_admin_profile()
{
  return &_admin_profile;
}

int profile_find(const char* name, user_profile_t** bprofile)
{
  int error;
  user_profile_t* profile;
  oss_node_t* node;
  oss_node_entry_t* entry;

  error = oss_node_find(_runtime_node, name, &entry);

  if (error)
    return error;

  if (entry->type != OSS_ENTRY_NESTED_NODE || !entry->node)
    return -KERR_NOT_FOUND;

  node = entry->node;

  if (node->type != OSS_PROFILE_NODE || !node->priv)
    return -KERR_NOT_FOUND;

  profile = node->priv;

  if (bprofile)
    *bprofile = profile;

  return 0;
}

/*!
 * @brief: Find a variable relative to the runtime node
 *
 * @path: The oss path relative to 'Runtime/'
 * @bvar: Buffer where the sysvar pointer will be put into on success
 */
int profile_find_var(const char* path, sysvar_t** bvar)
{
  int error;
  sysvar_t* var;
  oss_obj_t* obj;

  error = oss_resolve_obj_rel(_runtime_node, path, &obj);

  if (error)
    return error;

  if (obj->type != OSS_OBJ_TYPE_VAR || !obj->priv)
    return -KERR_INVAL;

  var = oss_obj_unwrap(obj, sysvar_t);

  /* Reference the variable */
  if (bvar)
    *bvar = get_sysvar(var);

  return KERR_NONE;
}

/*!
 * @brief: Get a sysvar relative to @profile
 */
int profile_get_var(user_profile_t* profile, const char* key, sysvar_t** bvar)
{
  int error;
  sysvar_t* var;
  oss_obj_t* obj;
  oss_node_entry_t* entry;

  error = oss_node_find(profile->node, key, &entry);

  if (error)
    return error;

  if (entry->type != OSS_ENTRY_OBJECT)
    return -KERR_INVAL;

  obj = entry->obj;

  if (obj->type != OSS_OBJ_TYPE_VAR || !obj->priv)
    return -KERR_INVAL;

  var = oss_obj_unwrap(obj, sysvar_t);

  if (bvar)
    *bvar = get_sysvar(var);

  return 0;
}

/*!
 * @brief: Add an environment to a user profile
 *
 * @returns: Negative error code on failure, new envid on success
 */
int profile_add_penv(user_profile_t* profile, struct penv* env)
{
  int error;

  if (!profile || !env)
    return -KERR_INVAL;

  if (env->profile)
    profile_remove_penv(env->profile, env);

  error = oss_node_add_node(profile->node, env->node);

  if (!error)
    env->profile = profile;

  return error;
}

/*!
 * @brief: Remove an environment from @profile
 */
int profile_remove_penv(user_profile_t* profile, struct penv* env)
{
  int error;
  oss_node_entry_t* entry = NULL;

  if (!profile || !env)
    return -KERR_INVAL;

  error = oss_node_remove_entry(profile->node, env->node->name, &entry);

  if (!error && entry) {
    destroy_oss_node_entry(entry);
    env->profile = nullptr;
  }

  return error;
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
int profile_set_password(user_profile_t* profile, const char* password)
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
int profile_clear_password(user_profile_t* profile)
{
  memset(profile->passwd_hash, 0, sizeof(profile->passwd_hash));
  profile->flags &= ~PROFILE_FLAG_HAS_PASSWD;
  return 0;
}

int profile_match_password(user_profile_t* profile, const char* match)
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
bool profile_has_password(user_profile_t* profile)
{
  return (profile->flags & PROFILE_FLAG_HAS_PASSWD) == PROFILE_FLAG_HAS_PASSWD;
}

/*!
 * @brief: Fill the Admin profile with default variables
 *
 * The BASE profile should store things that are relevant to system processes and contains
 * most of the variables that are not visible to userspace, like passwords, driver locations,
 * or general system file locations
 *
 * Little TODO, but still: The bootloader can also be aware of the files in the ramdisk when installing
 * the system to a harddisk partition. When it does, it will put drivers into their correct system directories 
 * and it can then create entries in the default BASE profile file (which we still need to load)
 */
static void __apply_admin_variables()
{
  sysvar_attach(_admin_profile.node, "KTERM_LOC", PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("Root/System/kterm.drv"));
  sysvar_attach(_admin_profile.node, DRIVERS_LOC_VARKEY, PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("Root/System/"));

  /* Core drivers which perform high level scanning and load low level drivers */
  sysvar_attach(_admin_profile.node, USBCORE_DRV_VARKEY, PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("usbcore.drv"));
  sysvar_attach(_admin_profile.node, "DISK_CORE_DRV", PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("diskcore.drv"));
  sysvar_attach(_admin_profile.node, "INPUT_CORE_DRV", PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("inptcore.drv"));
  sysvar_attach(_admin_profile.node, "VIDEO_CORE_DRV", PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("vidcore.drv"));
  sysvar_attach(_admin_profile.node, ACPICORE_DRV_VARKEY, PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("acpicore.drv"));
  sysvar_attach(_admin_profile.node, DYNLOADER_DRV_VARKEY, PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("dynldr.drv"));

  /* 
   * These variables are to store the boot parameters. When we've initialized all devices we need and we're
   * ready to load our bootdevice, we will look in "BASE/BOOT_DEVICE" to find the path to our bootdevice, after 
   * which we will verify that it has remained unchanged by checking if "BASE/BOOT_DEVICE_NAME" and "BASE/BOOT_DEVICE_SIZE"
   * are still valid.
   */
  sysvar_attach(_admin_profile.node, BOOT_DEVICE_VARKEY, PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR(NULL));
  sysvar_attach(_admin_profile.node, BOOT_DEVICE_SIZE_VARKEY, PRIV_LVL_ADMIN, SYSVAR_TYPE_QWORD, SYSVAR_FLAG_VOLATILE, NULL);
  sysvar_attach(_admin_profile.node, BOOT_DEVICE_NAME_VARKEY, PRIV_LVL_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR(NULL));

}

/*!
 * @brief: Fill the User profile with default variables
 *
 * These variables tell userspace about:
 * - the kernel its running on
 * - where it can find certain system files
 * - information about firmware
 * - information about devices (?)
 * - where default events are located
 * - where utility drivers are located
 */
static void __apply_user_variables()
{
  /* System name: Codename of the kernel that it present in the system */
  sysvar_attach(_user_profile.node, "SYSTEM_NAME", PRIV_LVL_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_CONSTANT | SYSVAR_FLAG_GLOBAL, PROFILE_STR("Aniva"));
  /* Major version number of the kernel */
  sysvar_attach(_user_profile.node, "VERSION_MAJ", PRIV_LVL_USER, SYSVAR_TYPE_BYTE, SYSVAR_FLAG_CONSTANT | SYSVAR_FLAG_GLOBAL, kernel_version.maj);
  /* Minor version number of the kernel */
  sysvar_attach(_user_profile.node, "VERSION_MIN", PRIV_LVL_USER, SYSVAR_TYPE_BYTE, SYSVAR_FLAG_CONSTANT | SYSVAR_FLAG_GLOBAL, kernel_version.min);
  /* Bump version number of the kernel. This is used to indicate small changes in between different builds of the kernel */
  sysvar_attach(_user_profile.node, "VERSION_BUMP", PRIV_LVL_USER, SYSVAR_TYPE_WORD, SYSVAR_FLAG_CONSTANT | SYSVAR_FLAG_GLOBAL, kernel_version.bump);

  /* Default provider for lwnd services */
  sysvar_attach(_user_profile.node, "DFLT_LWND_PATH", PRIV_LVL_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_GLOBAL, PROFILE_STR("service/lwnd"));
  /* Default eventname for the keyboard event */
  sysvar_attach(_user_profile.node, "DFLT_KB_EVENT", PRIV_LVL_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_GLOBAL, PROFILE_STR("keyboard"));
  /* Path variable to indicate default locations for executables */
  sysvar_attach(_user_profile.node, "PATH", PRIV_LVL_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_GLOBAL, PROFILE_STR("Root/Apps:Root/User/Apps"));
  sysvar_attach(_user_profile.node, LIBSPATH_VAR, PRIV_LVL_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_GLOBAL, PROFILE_STR("Root/System/Lib"));
  /* Name of the runtime library */
  sysvar_attach(_user_profile.node, "LIBRT_NAME", PRIV_LVL_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_GLOBAL, PROFILE_STR("librt.slb"));
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
void init_user_profiles(void)
{
  profile_mutex = create_mutex(NULL);
  _runtime_node = create_oss_node("Runtime", OSS_OBJ_STORE_NODE, NULL, NULL);

  ASSERT_MSG(KERR_OK(oss_attach_rootnode(_runtime_node)), "Failed to attach Runtime node to root!");

  init_proc_profile(&_admin_profile, "Admin", PRIV_LVL_ADMIN);
  init_proc_profile(&_user_profile, "User", PRIV_LVL_USER);

  /* Apply the default variables onto the default profiles */
  __apply_admin_variables();
  __apply_user_variables();
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
  global_prf_save_file = file_open(_user_profile.path);

  /* Shit, TODO: create this shit */
  if (!global_prf_save_file)
    return 0;

  kernel_panic("TODO: save_default_profiles");

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
    error = sysvarldr_load_variables(_user_profile.node, _user_profile.priv_level, f);

  file_close(f);

  if (!error)
    _user_profile.path = DEFAULT_GLOBAL_PVR_PATH;

  /* Create an eventhook on shutdown */
  kevent_add_hook("shutdown", "save default profiles", save_default_profiles);

  /* TMP: TODO remove */
  profile_set_password(&_admin_profile, "admin");
}
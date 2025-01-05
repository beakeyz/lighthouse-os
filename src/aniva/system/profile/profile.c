#include "profile.h"
#include "crypto/k_crc32.h"
#include "entry/entry.h"
#include "fs/file.h"
#include "kevent/types/profile.h"
#include "libk/flow/error.h"
#include "lightos/sysvar/shared.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "proc/core.h"
#include "proc/env.h"
#include "sync/mutex.h"
#include "system/profile/attr.h"
#include "system/profile/runtime.h"
#include "system/sysvar/loader.h"
#include "system/sysvar/map.h"
#include <kevent/event.h>
#include <libk/string.h>
#include <system/sysvar/var.h>

#define SOFTMAX_ACTIVE_PROFILES 4096
#define DEFAULT_USER_PVR_PATH "Root/Users/User/user.pvr"
#define DEFAULT_ADMIN_PVR_PATH "Root/Users/Admin/admin.pvr"

static mutex_t* _profile_mutex;

static uint32_t _active_profile_locked;
static user_profile_t* _active_profile;
static user_profile_t _admin_profile;
static user_profile_t _user_profile;
static oss_node_t* _runtime_node;

/*!
 * @brief: Initialize memory of a profile
 *
 * A single profile can store information about it's permissions and the variables
 * that are bound to this profile. These variables are stored in a variable sized
 * hashmap, that is not really optimized at all...
 */
int init_proc_profile(user_profile_t* profile, char* name, struct pattr* attr)
{
    if (!profile || !name)
        return -1;

    memset(profile, 0, sizeof(user_profile_t));

    profile->name = name;
    profile->lock = create_mutex(NULL);
    profile->node = create_oss_node(name, OSS_PROFILE_NODE, NULL, NULL);

    /* Initialize this profiles attributes */
    init_pattr(&profile->attr, attr ? attr->ptype : PROFILE_TYPE_LIMITED, attr ? attr->pflags : 0);

    /* Lock the profile in preperation for the attach */
    mutex_lock(profile->lock);

    /* FIXME: Error handle */
    (void)oss_node_add_node(_runtime_node, profile->node);

    /* Set the private field */
    profile->node->priv = profile;

    /* Release the lock */
    mutex_unlock(profile->lock);
    return 0;
}

static pattr_flags_t __base_flags[] = {
    [PROFILE_TYPE_ADMIN] = PATTR_ALL,
    [PROFILE_TYPE_SUPERVISOR] = PATTR_ALL,
    [PROFILE_TYPE_USER] = (PATTR_SEE | PATTR_READ | PATTR_WRITE),
    [PROFILE_TYPE_LIMITED] = (PATTR_SEE),
};

user_profile_t* create_proc_profile(char* name, enum PROFILE_TYPE type)
{
    user_profile_t* ret;
    pattr_t attr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    /* Initialize this boi */
    init_pattr(&attr, type, __base_flags);

    /* If we can init the profile, all good */
    if (KERR_OK(init_proc_profile(ret, name, &attr)))
        return ret;

    /* Fuck, free the memory and dip */
    kfree(ret);

    return nullptr;
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

user_profile_t* get_active_profile()
{
    user_profile_t* ret;

    /* Just in case someone is fucking us here lolol */
    mutex_lock(_profile_mutex);

    ret = _active_profile;

    /* Be nice and unlock */
    mutex_unlock(_profile_mutex);

    /* If the active profile gets changes right as we return, i'm very sad
     * TODO: Add a refcount to userprofiles, so we know when its safe to change
     * profile activity
     */
    return ret;
}

static inline bool _is_profile_activation_locked()
{
    return _active_profile_locked > 0;
}

/*!
 * @brief: Try to set an activated profile
 *
 * This tries to set an activated profile, if activation isn't locked
 */
int profile_set_activated(user_profile_t* profile)
{
    int error = -1;
    kevent_profile_ctx_t ctx;

    mutex_lock(_profile_mutex);

    if (_is_profile_activation_locked())
        goto unlock_and_exit;

    ctx.new = profile;
    ctx.old = _active_profile;
    ctx.type = KEVENT_PROFILE_CHANGE;

    /* Fire the profile change event */
    (void)kevent_fire("profile", &ctx, sizeof(ctx));

    /* Set the new profile */
    _active_profile = profile;
    error = 0;

unlock_and_exit:
    mutex_unlock(_profile_mutex);
    return error;
}

/*!
 * @brief: Lock profile activation
 *
 * Loads a random, non-null value into @key, which can then be used to unlock profile
 * activation again
 */
int profiles_lock_activation(uint32_t* key)
{
    if (!key || _is_profile_activation_locked())
        return -1;

    /* TODO: Randomness */
    mutex_lock(_profile_mutex);

    /* Just pretend this is random 0.0 */
    _active_profile_locked = 0x100;

    /* Set the key */
    *key = _active_profile_locked;

    mutex_unlock(_profile_mutex);
    return 0;
}

/*!
 * @brief: Unlock profile activation
 *
 * Checks if @key is equal to _active_profile_locked and resets this field
 * to zero if it is, in order to unlock profile activation
 */
int profiles_unlock_activation(uint32_t key)
{
    mutex_lock(_profile_mutex);

    /* Yikes bro, mismatch =/ */
    if (_is_profile_activation_locked() && key != _active_profile_locked) {
        mutex_unlock(_profile_mutex);
        return -1;
    }

    /* Reset =) */
    _active_profile_locked = NULL;

    mutex_unlock(_profile_mutex);
    return 0;
}

int profile_find_from(struct oss_node* rel_node, const char* name, user_profile_t** bprofile)
{
    int error;
    user_profile_t* profile;
    oss_node_t* node;
    oss_node_entry_t* entry;

    error = oss_node_find(rel_node, name, &entry);

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

int profile_find(const char* name, user_profile_t** bprofile)
{
    return profile_find_from(_runtime_node, name, bprofile);
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

    /* Inherit the profiles attributes */
    init_pattr(&env->attr, profile->attr.ptype, profile->attr.pflags);

    /* Mask away some unwanted things */
    pattr_mask(&env->attr, env->pflags_mask);

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
    return 0;
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
    sysvar_attach_ex(_admin_profile.node, "KTERM_LOC", PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("Root/System/kterm.drv"), 22);
    sysvar_attach_ex(_admin_profile.node, DRIVERS_LOC_VARKEY, PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("Root/System/"), 13);

    /* Core drivers which perform high level scanning and load low level drivers */
    sysvar_attach_ex(_admin_profile.node, USBCORE_DRV_VARKEY, PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("usbcore.drv"), 12);
    sysvar_attach_ex(_admin_profile.node, "DISK_CORE_DRV", PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("diskcore.drv"), 13);
    sysvar_attach_ex(_admin_profile.node, "INPUT_CORE_DRV", PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("inptcore.drv"), 13);
    sysvar_attach_ex(_admin_profile.node, "VIDEO_CORE_DRV", PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("vidcore.drv"), 12);
    sysvar_attach_ex(_admin_profile.node, ACPICORE_DRV_VARKEY, PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("acpicore.drv"), 13);
    sysvar_attach_ex(_admin_profile.node, DYNLOADER_DRV_VARKEY, PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR("dynldr.drv"), 11);

    /*
     * These variables are to store the boot parameters. When we've initialized all devices we need and we're
     * ready to load our bootdevice, we will look in "BASE/BOOT_DEVICE" to find the path to our bootdevice, after
     * which we will verify that it has remained unchanged by checking if "BASE/BOOT_DEVICE_NAME" and "BASE/BOOT_DEVICE_SIZE"
     * are still valid.
     */
    sysvar_attach_ex(_admin_profile.node, BOOT_DEVICE_VARKEY, PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR(NULL), 0);
    sysvar_attach_ex(_admin_profile.node, BOOT_DEVICE_SIZE_VARKEY, PROFILE_TYPE_ADMIN, SYSVAR_TYPE_QWORD, SYSVAR_FLAG_VOLATILE, NULL, 0);
    sysvar_attach_ex(_admin_profile.node, BOOT_DEVICE_NAME_VARKEY, PROFILE_TYPE_ADMIN, SYSVAR_TYPE_STRING, SYSVAR_FLAG_VOLATILE, PROFILE_STR(NULL), 0);
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
    sysvar_attach_ex(_user_profile.node, "SYSTEM_NAME", PROFILE_TYPE_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_CONSTANT | SYSVAR_FLAG_GLOBAL, ("Aniva"), 6);
    /* Major version number of the kernel */
    sysvar_attach_ex(_user_profile.node, "VERSION_MAJ", PROFILE_TYPE_USER, SYSVAR_TYPE_BYTE, SYSVAR_FLAG_CONSTANT | SYSVAR_FLAG_GLOBAL, &kernel_version.maj, 1);
    /* Minor version number of the kernel */
    sysvar_attach_ex(_user_profile.node, "VERSION_MIN", PROFILE_TYPE_USER, SYSVAR_TYPE_BYTE, SYSVAR_FLAG_CONSTANT | SYSVAR_FLAG_GLOBAL, &kernel_version.min, 1);
    /* Bump version number of the kernel. This is used to indicate small changes in between different builds of the kernel */
    sysvar_attach_ex(_user_profile.node, "VERSION_BUMP", PROFILE_TYPE_USER, SYSVAR_TYPE_WORD, SYSVAR_FLAG_CONSTANT | SYSVAR_FLAG_GLOBAL, &kernel_version.bump, 1);

    /* Default provider for lwnd services */
    sysvar_attach_ex(_user_profile.node, "DFLT_LWND_PATH", PROFILE_TYPE_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_GLOBAL, PROFILE_STR("service/lwnd"), 13);
    /* Path variable to indicate default locations for executables */
    sysvar_attach_ex(_user_profile.node, "PATH", PROFILE_TYPE_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_GLOBAL, PROFILE_STR("Root/Apps:Root/Users/User/Apps"), 31);
    sysvar_attach_ex(_user_profile.node, LIBSPATH_VAR, PROFILE_TYPE_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_GLOBAL, PROFILE_STR("Root/System/Lib"), 16);
    /* Name of the runtime library */
    sysvar_attach_ex(_user_profile.node, "LIBRT_NAME", PROFILE_TYPE_USER, SYSVAR_TYPE_STRING, SYSVAR_FLAG_GLOBAL, PROFILE_STR("librt.lib"), 10);
}

static void __apply_runtime_variables()
{
    sysvar_attach_ex(_runtime_node, RUNTIME_VARNAME_PROC_COUNT, PROFILE_TYPE_USER, SYSVAR_TYPE_DWORD, SYSVAR_FLAG_GLOBAL | SYSVAR_FLAG_STATIC, 0, 0);
}

uint32_t runtime_get_proccount()
{
    int error;
    uint32_t count;
    sysvar_t* var;

    /* Get the var */
    error = profile_find_var(RUNTIME_VARNAME_PROC_COUNT, &var);

    if (error)
        return 0;

    /* Read the value */
    count = sysvar_read_u32(var);

    /* Release our reference */
    release_sysvar(var);

    return count;
}

static inline void runtime_add_proccount_delta(int32_t delta)
{
    int error;
    int32_t count;
    sysvar_t* var;

    /* Get the var */
    error = profile_find_var(RUNTIME_VARNAME_PROC_COUNT, &var);

    if (error)
        return;

    /* Read the value */
    count = (i32)sysvar_read_u32(var) + delta;

    ASSERT_MSG((count + delta) >= 0, "runtime_add_proccount_delta -> Can't have a negative proc count!");

    /* Write delta to the sysvar */
    sysvar_write(var, (void*)&count, sizeof(u32));

    /* Release our reference */
    release_sysvar(var);
}

/*!
 * @brief: Add a proc to the proc count
 */
void runtime_add_proccount()
{
    runtime_add_proccount_delta(1);
}

/*!
 * @brief: Remove a proc from the proc count
 */
void runtime_remove_proccount()
{
    runtime_add_proccount_delta(-1);
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
    _profile_mutex = create_mutex(NULL);
    _runtime_node = create_oss_node("Runtime", OSS_OBJ_STORE_NODE, NULL, NULL);

    ASSERT_MSG(KERR_OK(oss_attach_rootnode(_runtime_node)), "Failed to attach Runtime node to root!");

    /* No active profile to start with */
    _active_profile_locked = 0;
    _active_profile = nullptr;

    /* Admin profile doesn't really care about permissons, since it overpowers all of them */
    init_proc_profile(&_admin_profile, "Admin", &(pattr_t) { .ptype = PROFILE_TYPE_ADMIN, .pflags = { 0 } });
    /* The user profile can do anything with any object of the same level by default, but lets limited plevels only look and read */
    init_proc_profile(&_user_profile, "User", &(pattr_t) { .ptype = PROFILE_TYPE_USER, .pflags = PATTR_FLAGS(0, 0, PATTR_ALL, PATTR_SEE | PATTR_READ) });

    /* Apply the default variables onto the default profiles */
    __apply_admin_variables();
    __apply_user_variables();

    __apply_runtime_variables();
}

/*!
 * @brief: Save current variables of the default profiles to disk
 *
 * If the files that contain the variables have not been created yet, we create them on the spot
 * There is a possibility that the system got booted with a ramdisk as a rootfs. In this case we can't
 * save any variables, since the system is in a read-only state. In order to save the variables, we'll
 * need to dynamically remount a filesystem with write capabilities.
 */
static int save_default_profiles(kevent_ctx_t* ctx, void* param)
{
    int error;
    file_t* global_prf_save_file;
    (void)ctx;

    /* Try to open the default file for the Global profile */
    global_prf_save_file = file_open(_user_profile.image_path);

    /* Shit, TODO: create this shit */
    if (!global_prf_save_file)
        return 0;

    /* Do the save */
    error = sysvarldr_save_variables(_user_profile.node, 0, global_prf_save_file);

    file_close(global_prf_save_file);
    return error;
}

/*!
 * @brief: Default handler for generic profile events
 *
 * TODO: handle
 */
static int default_profile_handler(kevent_ctx_t* _ctx, void* param)
{
    kevent_profile_ctx_t* ctx;

    if (_ctx->buffer_size != sizeof(*ctx))
        return 0;

    ctx = _ctx->buffer;

    switch (ctx->type) {
    case KEVENT_PROFILE_CHANGE:
    case KEVENT_PROFILE_PRIV_CHANGE:
        break;
    }

    return 0;
}

static inline void __profile_load_variables(user_profile_t* profile, const char* path)
{
    int error = -1;
    file_t* f;

    if (!profile || !path)
        return;

    f = file_open(path);

    if (!f)
        return;

    error = sysvarldr_load_variables(profile->node, profile->attr.ptype, f);

    file_close(f);

    if (!error)
        profile->image_path = path;
}

/*!
 * @brief: Late initialization of the profiles
 *
 * This keeps itselfs busy with any profile variables that where saved on disk. It also creates a shutdown eventhook
 * to save it's current default profiles to disk
 */
void init_profiles_late(void)
{
    __profile_load_variables(&_user_profile, DEFAULT_USER_PVR_PATH);
    __profile_load_variables(&_admin_profile, DEFAULT_ADMIN_PVR_PATH);

    /* Create an eventhook for shutdown */
    kevent_add_hook("shutdown", "save default profiles", save_default_profiles, NULL);
    kevent_add_hook("profile", "default profile handler", default_profile_handler, NULL);

    /* TMP: TODO remove */
    profile_set_password(&_admin_profile, "admin");
}

#ifndef __ANIVA_PROC_PROFILE__
#define __ANIVA_PROC_PROFILE__

#include "system/sysvar/map.h"
#include "system/sysvar/var.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct proc;
struct oss_node;

/*
 * Aniva user (profile) management
 *
 * Profiles manage permissions, privilege levels, ownership over files and stuff,
 * ect. There are two head profiles that get loaded with the systemboot:
 *  - Admin profile: full permissions, anything is kernel-owned
 *  - User profile: basic permissions
 * But more profiles can be loaded / created by userspace and stored on disk. These profiles really are files, 
 * but they aren't stored on the vfs, so userspace will have to interract with them through special kernel
 * interfaces. (Of cource handles can still be obtained)
 */

/* Most powerful privilege level */
#define PRIV_LVL_ADMIN 0xFF
/* Most basic privilege level */
#define PRIV_LVL_USER 0
/* Just an invalid value that does not fit in the field
 NOTE: BE CAREFUL WITH THIS */
#define PRIV_LVL_NONE 0x100

#define MAX_VARS 4096

/* Processes in this profile may load drivers */
#define PRF_PERM_LOAD_DRV (1ULL << 0)
/* Processes in this profile may unload drivers */
#define PRF_PERM_UNLOAD_DRV (1ULL << 1)

/* This profile has a password in the hash */
#define PROFILE_FLAG_HAS_PASSWD 0x00000001UL

#define PROFILE_STR(str) (uint64_t)(str)

#define DRIVERS_LOC_VARKEY "DRIVERS_LOC"
#define BOOT_DEVICE_VARKEY "BOOT_DEVICE"
#define BOOT_DEVICE_SIZE_VARKEY "BOOT_DEVICE_SIZE"
#define BOOT_DEVICE_NAME_VARKEY "BOOT_DEVICE_NAME"

#define ACPICORE_DRV_VARKEY "ACPI_CORE_DRV"
#define USBCORE_DRV_VARKEY "USB_CORE_DRV"
#define DYNLOADER_DRV_VARKEY "DYN_LDR_DRV"

#define ACPICORE_DRV_VAR_PATH "Admin/"ACPICORE_DRV_VARKEY
#define USBCORE_DRV_VAR_PATH "Admin/"USBCORE_DRV_VARKEY
#define DYNLOADER_DRV_VAR_PATH "Admin/"DYNLOADER_DRV_VARKEY

#define DRIVERS_LOC_VAR_PATH "Admin/"DRIVERS_LOC_VARKEY
#define BOOT_DEVICE_VAR_PATH "Admin/"BOOT_DEVICE_VARKEY
#define BOOT_DEVICE_SIZE_VAR_PATH "Admin/"BOOT_DEVICE_SIZE_VARKEY
#define BOOT_DEVICE_NAME_VAR_PATH "Admin/"BOOT_DEVICE_NAME_VARKEY

#define LIBSPATH_VAR "LIBSPATH"
#define LIBSPATH_VARPATH "User/"LIBSPATH_VAR

#define PATH_SEPERATOR_CHAR ':'

/*
 * Profiles need to be saveable and loadable since they will 
 * act as our 'users' AND our 'groups' at the same time
 *
 * TODO: find out how to load profiles from files and save profiles to files
 *
 * Other things profiles need to be able to do:
 * - 'logged in' and 'logged out': They need to be able to hold their credentials in a secure way
 */
typedef struct user_profile {
  const char* name;
  /* In the case of a custom profile, loaded from a file */
  const char* path;
  /* This node holds both the environments of this profile AND the variables local to the profile */
  struct oss_node* node;

  uint16_t priv_level;
  /* Normal profile flags */
  uint32_t flags;
  uint32_t env_count;

  /* 
   * Hash for the profiles password 
   * The current algorithm is crc32, but we need something more
   * secure lmaoooo
   */
  uint64_t passwd_hash[2];

  mutex_t* lock;
} user_profile_t;

void init_user_profiles(void);
void init_profiles_late(void);

static inline void profile_set_priv_lvl(user_profile_t* profile, uint8_t lvl)
{
  /* Set raw here, so we overwrite garbage bits while setting the check */
  profile->priv_level = lvl;
}

static inline bool profile_is_from_file(user_profile_t* profile)
{
  return (profile->path != nullptr);
}

int init_proc_profile(user_profile_t* profile, char* name, uint32_t level);
user_profile_t* create_proc_profile(char* name, uint8_t level);
void destroy_proc_profile(user_profile_t* profile);

user_profile_t* get_user_profile();
user_profile_t* get_admin_profile();

int profile_find(const char* name, user_profile_t** bprofile);
int profile_find_var(const char* path, struct sysvar** var);
int profile_get_var(user_profile_t* profile, const char* key, struct sysvar** var);
int profile_add_penv(user_profile_t* profile, struct penv* env);
int profile_remove_penv(user_profile_t* profile, struct penv* env);

int profile_set_password(user_profile_t* profile, const char* password);
int profile_clear_password(user_profile_t* profile);
int profile_match_password(user_profile_t* profile, const char* match);
bool profile_has_password(user_profile_t* profile);

#endif // !__ANIVA_PROC_PROFILE__

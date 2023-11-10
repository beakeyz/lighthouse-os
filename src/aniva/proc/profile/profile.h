#ifndef __ANIVA_PROC_PROFILE__
#define __ANIVA_PROC_PROFILE__

#include "fs/file.h"
#include "libk/data/hashmap.h"
#include "proc/profile/variable.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct proc;

/*
 * Aniva user (profile) management
 *
 * Profiles manage permissions, privilege levels, ownership over files and stuff,
 * ect. There are two head profiles that get loaded with the systemboot:
 *  - base profile: full permissions, anything is kernel-owned
 *  - glbl profile: basic permissions
 * But more profiles can be loaded / created by userspace and stored on disk. These profiles really are files, 
 * but they aren't stored on the vfs, so userspace will have to interract with them through special kernel
 * interfaces. (Of cource handles can still be obtained)
 */

/* Most powerful privilege level */
#define PRF_PRIV_LVL_BASE 0
#define PRF_PRIV_LVL_ADMIN 3
/* Most basic privilege level */
#define PRF_PRIV_LVL_USER 16

#define PRF_MAX_VARS 1024 

/* Processes in this profile may load drivers */
#define PRF_PERM_LOAD_DRV (1 << 0)
/* Processes in this profile may unload drivers */
#define PRF_PERM_UNLOAD_DRV (1 << 1)

/*
 * Profiles need to be saveable and loadable since they will 
 * act as our 'users' AND our 'groups' at the same time
 *
 * TODO: find out how to load profiles from files and save profiles to files
 *
 * Other things profiles need to be able to do:
 * - 'logged in' and 'logged out': They need to be able to hold their credentials in a secure way
 */
typedef struct proc_profile {
  const char* name;

  /* In the case of a custom profile, loaded from a file */
  const char* path;

  /*
   * Privilege level is checked with
   * the oposite bits of the lvl
   */
  union {
    struct {
      uint8_t lvl;
      uint8_t check;
    };
    uint16_t raw;
  } priv_level;

  /* Profile permission flags */
  uint64_t permission_flags;
  /* Normal profile flags */
  uint32_t flags;
  uint32_t proc_count;

  mutex_t* lock;
  hashmap_t* var_map;
} proc_profile_t;

void init_proc_profiles(void);

static inline bool profile_is_valid_priv_level(proc_profile_t* profile)
{
  return (profile->priv_level.check == ~(profile->priv_level.lvl));
}

static inline void profile_set_priv_lvl(proc_profile_t* profile, uint8_t lvl)
{
  /* Set raw here, so we overwrite garbage bits while setting the check */
  profile->priv_level.lvl = lvl;
  profile->priv_level.check = ~(profile->priv_level.lvl);
}

static inline uint8_t profile_get_priv_lvl(proc_profile_t* profile)
{
  return profile->priv_level.lvl;
}

static inline bool profile_is_from_file(proc_profile_t* profile)
{
  return (profile->path != nullptr);
}

void init_proc_profile(proc_profile_t* profile, char* name, uint8_t level);
void destroy_proc_profile(proc_profile_t* profile);

int profile_find(const char* name, proc_profile_t** profile);
int profile_foreach(hashmap_itterate_fn_t fn);

int profile_add_var(proc_profile_t* profile, profile_var_t* var);
int profile_remove_var(proc_profile_t* profile, const char* key);
int profile_get_var(proc_profile_t* profile, const char* key, profile_var_t** var);

int profile_scan_var(const char* path, proc_profile_t** profile, profile_var_t** var);

bool profile_can_see_var(proc_profile_t* profile, profile_var_t* var);

int profile_register(proc_profile_t* profile);
int profile_unregister(const char* name);

int profile_set_password(proc_profile_t* profile, const char* password);
int profile_clear_password(proc_profile_t* profile);
int profile_get_password_len(proc_profile_t* profile, size_t* size);
int profile_get_password(proc_profile_t* profile, uint8_t* buffer, size_t size);
bool profile_has_password(proc_profile_t* profile);
bool profile_has_valid_password_var(proc_profile_t* profile);

/*
 * Functions to bind processes to the default profiles
 * in order to bind to other profiles, use the function
 * <proc_set_profile> in proc.c (proc.h)
 */
int proc_register_to_base(struct proc* p);
int proc_register_to_global(struct proc* p);

/* loader.c */
extern int profile_save(proc_profile_t* profile, file_t* file);
extern int profile_load(proc_profile_t** profile, file_t* file);
extern int profile_save_variables(proc_profile_t* profile, file_t* file);
extern int profile_load_variables(proc_profile_t* profile, file_t* file);

uint16_t get_active_profile_count();

#endif // !__ANIVA_PROC_PROFILE__

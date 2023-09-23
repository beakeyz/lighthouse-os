#ifndef __ANIVA_PROC_PROFILE__
#define __ANIVA_PROC_PROFILE__

#include <libk/stddef.h>

/*
 * Aniva user (profile) management
 *
 * Profiles manage permissions, privilege levels, ownership over files and stuff,
 * ect. There are two head profiles that get loaded with the systemboot:
 *  - admn profile: full permissions, anything is kernel-owned
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

typedef struct proc_profile {
  const char* name;

  /* In the case of a custom profile */
  const char* path;

  /*
   * Privilege level is checked with
   * the oposite bits of the lvl
   */
  union {
    struct {
      uint8_t lvl: 4;
      uint8_t check: 4;
    };
    uint8_t raw;
  } priv_level;

} proc_profile_t;

static inline bool profile_is_valid_priv_level(proc_profile_t* profile)
{
  return (~(profile->priv_level.check) == profile->priv_level.lvl);
}

static inline void profile_set_priv_check(proc_profile_t* profile)
{
  profile->priv_level.check = ~(profile->priv_level.lvl);
}

void init_proc_profile(proc_profile_t* profile, char* name, uint8_t level);

#endif // !__ANIVA_PROC_PROFILE__

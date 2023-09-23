#include "profile.h"
#include <libk/string.h>

void init_proc_profile(proc_profile_t* profile, char* name, uint8_t level)
{
  if (!profile || !name || (level > 0x0f))
    return;

  memset(profile, 0, sizeof(proc_profile_t));

  profile->name = name;
  profile->priv_level.lvl = level;

  profile_set_priv_check(profile);
}

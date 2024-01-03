#ifndef __TOOLS_BASE_ENV_DEFAULTS__
#define __TOOLS_BASE_ENV_DEFAULTS__

#include "lightos/proc/var_types.h"
#include <stdint.h>

struct profile_var_template {
  const char* key;
  enum PROFILE_VAR_TYPE type;
  void* value;
};

#define VAR_ENTRY(key, type, value) { key, type, value }
#define VAR_NUM(num) (void*)(num)

extern struct profile_var_template base_defaults[];
extern struct profile_var_template global_defaults[];

#endif // !__TOOLS_BASE_ENV_DEFAULTS__

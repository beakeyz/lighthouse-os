#ifndef __TOOLS_BASE_ENV_DEFAULTS__
#define __TOOLS_BASE_ENV_DEFAULTS__

#include "proc/var_types.h"
#include <stdint.h>

struct profile_var_template {
  const char* key;
  enum PROFILE_VAR_TYPE type;
  void* value;
  uint32_t flags;
};

#define VAR_ENTRY(key, type, value, flags) { key, type, value, flags }
#define VAR_NUM(num) (void*)(num)

#define DEFAULT_VAR_CAPACITY     0x1000
#define DEFAULT_STRTAB_BYTECOUNT 0x4000
#define DEFAULT_VALTAB_CAPACITY  0x1000

extern struct profile_var_template base_defaults[];
extern struct profile_var_template global_defaults[];

#endif // !__TOOLS_BASE_ENV_DEFAULTS__

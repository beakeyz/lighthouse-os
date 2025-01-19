#ifndef __TOOLS_BASE_ENV_DEFAULTS__
#define __TOOLS_BASE_ENV_DEFAULTS__

#include "lightos/api/sysvar.h"
#include <stdint.h>

typedef struct __sysvar_template {
    const char* key;
    enum SYSVAR_TYPE type;
    uint32_t flags;
    void* value;
} base_sysvar_template;

#define VAR_ENTRY(key, type, value, flags) \
    {                                      \
        key, type, flags, (void*)(value)   \
    }
#define VAR_NUM(num) (void*)(num)

#define DEFAULT_VAR_CAPACITY 0x1000
#define DEFAULT_STRTAB_BYTECOUNT 0x4000

extern struct __sysvar_template base_defaults[];
extern struct __sysvar_template global_defaults[];

#endif // !__TOOLS_BASE_ENV_DEFAULTS__

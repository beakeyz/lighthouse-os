#ifndef __TOOLS_BASE_ENV_DEFAULTS__
#define __TOOLS_BASE_ENV_DEFAULTS__

#include <stdint.h>
#include <sysvar/shared.h>

struct sysvar_template {
    const char* key;
    enum SYSVAR_TYPE type;
    void* value;
    uint32_t flags;
};

#define VAR_ENTRY(key, type, value, flags) \
    {                                      \
        key, type, (void*)(value), flags   \
    }
#define VAR_NUM(num) (void*)(num)

#define DEFAULT_VAR_CAPACITY 0x1000
#define DEFAULT_STRTAB_BYTECOUNT 0x4000

extern struct sysvar_template base_defaults[];
extern struct sysvar_template global_defaults[];

#endif // !__TOOLS_BASE_ENV_DEFAULTS__

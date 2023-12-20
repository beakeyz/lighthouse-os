#ifndef __ANIVA_SYS_OPEN__
#define __ANIVA_SYS_OPEN__

#include "LibSys/proc/var_types.h"
#include <libk/stddef.h>
#include <LibSys/handle_def.h>

HANDLE sys_open(const char* __user path, HANDLE_TYPE type, uint32_t flags, uint32_t mode);

HANDLE sys_open_file(const char* __user path, uint16_t flags, uint32_t mode);
HANDLE sys_open_proc(const char* __user name, uint16_t flags, uint32_t mode);
HANDLE sys_open_driver(const char* __user name, uint16_t flags, uint32_t mode);

HANDLE sys_open_pvar(const char* __user name, HANDLE profile_handle, uint16_t flags);
bool sys_create_pvar(HANDLE profile_handle, const char* __user name, enum PROFILE_VAR_TYPE type, uint32_t flags, void* __user value);

uintptr_t sys_close(HANDLE handle);

#endif // !__ANIVA_SYS_OPEN__

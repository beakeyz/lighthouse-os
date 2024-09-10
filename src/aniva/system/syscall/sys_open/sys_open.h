#ifndef __ANIVA_SYS_OPEN__
#define __ANIVA_SYS_OPEN__

#include "lightos/sysvar/shared.h"
#include <libk/stddef.h>
#include <lightos/handle_def.h>

HANDLE sys_open(const char* __user path, HANDLE_TYPE type, uint32_t flags, uint32_t mode);
HANDLE sys_open_rel(HANDLE rel_handle, const char* __user path, HANDLE_TYPE type, uint32_t flags, uint32_t mode);
vaddr_t sys_get_funcaddr(HANDLE lib_handle, const char __user* path);

bool sys_create_pvar(HANDLE profile_handle, const char* __user name, enum SYSVAR_TYPE type, uint32_t flags, void* __user value);

uintptr_t sys_close(HANDLE handle);

#endif // !__ANIVA_SYS_OPEN__

#ifndef __ANIVA_SYS_OPEN__
#define __ANIVA_SYS_OPEN__

#include <libk/stddef.h>
#include <LibSys/handle_def.h>

HANDLE sys_open(const char* __user path, HANDLE_TYPE type, uint16_t flags, uint32_t mode);

HANDLE sys_open_file(const char* __user path, uint16_t flags, uint32_t mode);
HANDLE sys_open_proc(const char* __user name, uint16_t flags, uint32_t mode);
HANDLE sys_open_driver(const char* __user name, uint16_t flags, uint32_t mode);

HANDLE sys_open_pvar(const char* __user name, HANDLE profile_handle, uint16_t flags);

uintptr_t sys_close(HANDLE handle);

#endif // !__ANIVA_SYS_OPEN__

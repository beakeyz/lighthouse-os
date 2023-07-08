#ifndef __ANIVA_SYS_OPEN__
#define __ANIVA_SYS_OPEN__

#include <libk/stddef.h>
#include <LibSys/handle_def.h>

HANDLE_t sys_open(const char* __user path, HANDLE_TYPE_t type, uint16_t flags, uint32_t mode);

HANDLE_t sys_open_file(const char* __user path, uint16_t flags, uint32_t mode);
HANDLE_t sys_open_proc(const char* __user name, uint16_t flags, uint32_t mode);
HANDLE_t sys_open_driver(const char* __user name, uint16_t flags, uint32_t mode);

#endif // !__ANIVA_SYS_OPEN__

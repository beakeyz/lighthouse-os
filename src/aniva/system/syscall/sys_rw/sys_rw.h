#ifndef __ANIVA_SYS_READWRITE__
#define __ANIVA_SYS_READWRITE__

#include <libk/stddef.h>
#include "lightos/handle_def.h"

uint64_t sys_read(handle_t handle, uint8_t __user* buffer, size_t length);
uint64_t sys_write(handle_t handle, uint8_t __user* buffer, size_t length);

/* TODO */
uint64_t sys_seek(handle_t handle, uintptr_t offset, uint32_t type);

#endif // !__ANIVA_SYS_READWRITE__

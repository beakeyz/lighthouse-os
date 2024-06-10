#ifndef __ANIVA_SYS_READWRITE__
#define __ANIVA_SYS_READWRITE__

#include "lightos/handle_def.h"
#include <libk/stddef.h>

uint64_t sys_read(handle_t handle, uint8_t __user* buffer, size_t length);
uint64_t sys_write(handle_t handle, uint8_t __user* buffer, size_t length);

/* TODO */
uint64_t sys_seek(handle_t handle, uintptr_t offset, uint32_t type);

uint64_t sys_dir_read(handle_t handle, uint32_t idx, char __user* namebuffer, size_t blen);

#endif // !__ANIVA_SYS_READWRITE__

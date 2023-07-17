#ifndef __ANIVA_SYS_READWRITE__
#define __ANIVA_SYS_READWRITE__

#include <libk/stddef.h>
#include "LibSys/handle_def.h"

uint64_t sys_read(handle_t handle, uint8_t __user* buffer, size_t length);
uint64_t sys_write(handle_t handle, uint8_t __user* buffer, size_t length);

#endif // !__ANIVA_SYS_READWRITE__

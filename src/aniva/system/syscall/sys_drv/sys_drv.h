#ifndef __ANIVA_SYS_IOCTL__
#define __ANIVA_SYS_IOCTL__
#include "LibSys/handle_def.h"
#include "dev/core.h"
#include <libk/stddef.h>

uintptr_t sys_send_ioctl(HANDLE handle, driver_control_code_t code, void* buffer, size_t size);
uintptr_t sys_get_handle_type(HANDLE handle);

#endif // !__ANIVA_SYS_IOCTL__

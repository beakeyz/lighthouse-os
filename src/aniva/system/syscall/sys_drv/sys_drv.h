#ifndef __ANIVA_SYS_IOCTL__
#define __ANIVA_SYS_IOCTL__
#include "dev/core.h"
#include "lightos/dev/shared.h"
#include "lightos/handle_def.h"
#include "lightos/sysvar/shared.h"
#include <libk/stddef.h>

#include <lightos/driver/ctl.h>

uintptr_t sys_send_message(HANDLE handle, driver_control_code_t code, void* buffer, size_t size);
uintptr_t sys_send_ctl(HANDLE handle, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t bsize);
u64 sys_unload_driver(HANDLE handle);

bool sys_get_pvar_type(HANDLE pvar_handle, enum SYSVAR_TYPE* type_buffer);
uintptr_t sys_get_handle_type(HANDLE handle);

#endif // !__ANIVA_SYS_IOCTL__

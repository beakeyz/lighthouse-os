#ifndef __ANIVA_SYS_IOCTL__
#define __ANIVA_SYS_IOCTL__
#include "LibSys/handle_def.h"
#include "LibSys/proc/var_types.h"
#include "dev/core.h"
#include <libk/stddef.h>

#include <LibSys/driver/ctl.h>

uintptr_t sys_send_message(HANDLE handle, driver_control_code_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

uintptr_t sys_send_drv_ctl(HANDLE handle, enum DRV_CTL_MODE mode);

bool sys_get_pvar_type(HANDLE pvar_handle, enum PROFILE_VAR_TYPE* type_buffer);
uintptr_t sys_get_handle_type(HANDLE handle);

#endif // !__ANIVA_SYS_IOCTL__

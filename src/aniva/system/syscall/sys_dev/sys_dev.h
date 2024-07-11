#ifndef __ANIVA_SYSCALLS_DEVICE__
#define __ANIVA_SYSCALLS_DEVICE__

#include "devices/shared.h"
#include "lightos/handle_def.h"
#include <libk/stddef.h>

uintptr_t sys_get_devinfo(HANDLE handle, DEVINFO* binfo);
uintptr_t sys_enable_device(HANDLE handle, bool enable);

#endif // !__ANIVA_SYSCALLS_DEVICE__

#ifndef __ANIVA_SYSCALL_SYS_PROC__
#define __ANIVA_SYSCALL_SYS_PROC__

#include "lightos/handle_def.h"
#include <libk/stddef.h>

u64 sys_create_proc(const char __user* name, FuncPtr __user entry);
u64 sys_destroy_proc(HANDLE proc, u32 flags);

#endif // !__ANIVA_SYSCALL_SYS_PROC__

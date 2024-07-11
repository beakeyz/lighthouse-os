#ifndef __ANIVA_SYSCALL_SYS_PROC__
#define __ANIVA_SYSCALL_SYS_PROC__

#include "lightos/handle_def.h"
#include <libk/stddef.h>

u64 sys_create_proc(const char __user* cmd, FuncPtr __user entry);
u64 sys_destroy_proc(HANDLE proc, u32 flags);

uintptr_t sys_get_process_time_ms();

#endif // !__ANIVA_SYSCALL_SYS_PROC__

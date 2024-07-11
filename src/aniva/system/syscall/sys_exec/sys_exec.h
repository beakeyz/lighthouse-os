#ifndef __ANIVA_SYS_EXEC__
#define __ANIVA_SYS_EXEC__

/*
 * Sys-exec
 *
 * Static syscall entries that revolve around processes and their environment
 */

#include <libk/stddef.h>

uintptr_t sys_exec(char __user* cmd, size_t cmd_len);
uintptr_t sys_sleep(uintptr_t ms);

#endif // !__ANIVA_SYS_EXEC__

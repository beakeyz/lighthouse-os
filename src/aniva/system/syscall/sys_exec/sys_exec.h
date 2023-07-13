#ifndef __ANIVA_SYS_EXEC__
#define __ANIVA_SYS_EXEC__

#include <libk/stddef.h>

uintptr_t sys_exec(char __user* cmd, size_t cmd_len);

#endif // !__ANIVA_SYS_EXEC__

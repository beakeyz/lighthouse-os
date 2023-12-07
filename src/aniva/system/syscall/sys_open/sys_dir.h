#ifndef __ANIVA_SYSCALL_DIR__
#define __ANIVA_SYSCALL_DIR__

#include "LibSys/handle_def.h"
#include <libk/stddef.h>

int sys_create_dir(const char* __user pathname, int mode);

#endif // !__ANIVA_SYSCALL_DIR__

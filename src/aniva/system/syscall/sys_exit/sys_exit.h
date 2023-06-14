#ifndef __ANIVA_SYS_EXIT__
#define __ANIVA_SYS_EXIT__

/*
 * Main entrypoint for the exit syscall
 */

#include <libk/stddef.h>

uintptr_t sys_exit_handler(uintptr_t code);

#endif // !__ANIVA_SYS_EXIT__

#ifndef __LIGHTENV_SYSTEM__
#define __LIGHTENV_SYSTEM__

#include <lightos/handle_def.h>
#include <sys/types.h>

/*
 * Syscalls
 *
 * This header defines a framework for defining the absolute most basic form of syscall definitions
 * which simply execute the syscall instruction for the architecture we support and build for
 *
 * We use a different calling convention to System-V, since syscall uses some registers to store different things,
 * namely uses rcx and r11
 * rax: function
 * rbx: arg0
 * rdx: arg1
 * rdi: arg2
 * rsi: arg3
 * r8:  arg4
 *
 *
 * TODO: syscall tracing
 */

#define SYS_MAXARGS (5)

typedef uintptr_t syscall_id_t;
typedef unsigned long long syscall_result_t;

#endif // !__LIGHTENV_SYSTEM__

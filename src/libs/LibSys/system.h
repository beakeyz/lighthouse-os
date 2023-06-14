#ifndef __LIGHTENV_SYSTEM__
#define __LIGHTENV_SYSTEM__

#include <LibDef/def.h>

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

#define SYS_OK              (0)
#define SYS_INV             (1)
#define SYS_KERR            (2)
#define SYS_NOENT           (3)
#define SYS_NOPERM          (4)
#define SYS_NULL            (5)
#define SYS_ERR             (-1)

#define SYS_0ARG            (0)
#define SYS_1ARG            (1)
#define SYS_2ARG            (2)
#define SYS_3ARG            (3)
#define SYS_4ARG            (4)
#define SYS_5ARG            (5)

#define SYS_MAXARGS         (5)

typedef uintptr_t syscall_id_t;
typedef int syscall_result_t;

/* Most basic syscall */
syscall_result_t syscall_x(
  syscall_id_t id,
  size_t argc,
  uintptr_t arg0,
  uintptr_t arg1,
  uintptr_t arg2,
  uintptr_t arg3,
  uintptr_t arg4
);

#endif // !__LIGHTENV_SYSTEM__

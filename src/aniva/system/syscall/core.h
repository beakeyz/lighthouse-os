#ifndef __ANIVA_SYSCALL_CORE__
#define __ANIVA_SYSCALL_CORE__
#include "libk/flow/error.h"
#include "lightos/syscall.h"
#include <libk/stddef.h>

/*
 * This header represents the core of the kernel syscall system
 * evey type of syscall has an ID and we can register additional syscall handlers
 * by giving them these functions that we then call in to when the syscall
 * is invoked.
 */

typedef uint64_t (*sys_fn_t)(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4);

#define SYSCALL_CALLED (0x00000001)
#define SYSCALL_CUSTOM (0x00000002)

/*
 * Concepts for syscalls
 *
 * o Syslinks -> bind syscalls to different sources. Allows for 'custom' or 'dynamic' syscalls by drivers
 * o Call counting -> track syscall call counts per process
 */
typedef struct syscall {
    uint32_t m_flags;
    enum SYSID m_id;
    sys_fn_t m_handler;
} syscall_t;

void sys_entry();

kerror_t install_syscall(uint32_t id, sys_fn_t handler);
kerror_t uninstall_syscall(uint32_t id);

uintptr_t call_syscall(uint32_t id, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);

#endif //__ANIVA_SYSCALL_CORE__

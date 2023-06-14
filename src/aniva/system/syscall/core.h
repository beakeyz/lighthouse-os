#ifndef __ANIVA_SYSCALL_CORE__
#define __ANIVA_SYSCALL_CORE__
#include <libk/stddef.h>
#include "system/processor/registers.h"
#include "system/processor/processor_info.h"
#include "proc/thread.h"

/*
 * This header represents the core of the kernel syscall system
 * evey type of syscall has an ID and we can register additional syscall handlers
 * by giving them these functions that we then call in to when the syscall
 * is invoked.
 */

typedef uintptr_t syscall_id_t;

typedef uint64_t (*sys_fn)(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4
);

typedef struct syscall {
  syscall_id_t m_id;
  sys_fn m_handler;
} syscall_t;

extern void sys_handler(registers_t* regs);
extern NAKED void sys_entry();

bool test_syscall_capabilities(processor_info_t* info);


#endif //__ANIVA_SYSCALL_CORE__

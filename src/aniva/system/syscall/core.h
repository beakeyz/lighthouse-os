#ifndef __ANIVA_SYSCALL_CORE__
#define __ANIVA_SYSCALL_CORE__
#include <libk/stddef.h>
#include "system/processor/registers.h"
#include "system/processor/processor_info.h"
#include "proc/thread.h"

/*
 * TODO: figure out lmao
 */

enum SYSCALLS {
  EXIT
};

struct syscall {
  thread_t* m_caller_thread;
  // timestamp

};

extern void sys_handler(registers_t* regs);
extern NAKED void sys_entry();

bool test_syscall_capabilities(processor_info_t* info);


#endif //__ANIVA_SYSCALL_CORE__

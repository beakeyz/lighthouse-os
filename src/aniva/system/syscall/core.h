#ifndef __ANIVA_SYSCALL_CORE__
#define __ANIVA_SYSCALL_CORE__
#include <libk/stddef.h>
#include "system/processor/registers.h"
#include "system/processor/processor_info.h"
#include "proc/thread.h"

/*
 * TODO: figure out lmao
 */

enum SYSCALL_ID {
  EXIT = 0,
  OPEN,
  CLOSE,
  GETTIME,
};

typedef int64_t (*sys_fn)(int64_t, int64_t, int64_t, int64_t);

struct syscall {
  enum SYSCALL_ID m_id;
  sys_fn m_handler;
};

extern void sys_handler(registers_t* regs);
extern NAKED void sys_entry();

bool test_syscall_capabilities(processor_info_t* info);


#endif //__ANIVA_SYSCALL_CORE__

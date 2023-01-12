#ifndef __LIGHT_THREAD__
#define __LIGHT_THREAD__
#include "libk/error.h"
#include "mem/PagingComplex.h"
#include "proc/context.h"
#include "proc/proc.h"
#include "system/processor/fpu/state.h"
#include "system/processor/gdt.h"
#include <system/processor/registers.h>
#include <libk/stddef.h>

typedef enum {
  INVALID = 0,
  RUNNING,
  RUNNABLE,
  DYING,
  DEAD,
  STOPPED,
  BLOCKED,
} ThreadState;


typedef struct {
  kContext_t m_context;

  char m_name[32];
  uint32_t m_cpu; // atomic?
  uint32_t m_ticks_elapsed;
  uint32_t m_max_ticks;

  FpuState m_fpu_state;

  uintptr_t m_stack_top;

  ThreadState m_current_state;

  proc_t* m_parent_proc; // nullable

} thread_t;

LIGHT_STATUS create_thread(thread_t*, FuncPtr, const char*, bool); // make this sucka

LIGHT_STATUS kill_thread(thread_t*); // kill thread and prepare for context swap to kernel
LIGHT_STATUS kill_thread_if(thread_t*, bool); // kill if condition is met
LIGHT_STATUS clean_thread(thread_t*); // clean resources thead used

LIGHT_STATUS switch_context_to(thread_t*);

void reset_thread_fpu_state(thread_t*);

#endif // !__LIGHT_THREAD__

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
  NO_CONTEXT,
} ThreadState;


typedef struct {
  kContext_t m_context;

  char m_name[32];
  uint32_t m_cpu; // atomic?
  uint32_t m_ticks_elapsed;
  uint32_t m_max_ticks;

  FpuState m_fpu_state;

  __attribute__((aligned(16))) uintptr_t m_stack_top;

  ThreadState m_current_state;

  proc_t* m_parent_proc; // nullable

} thread_t;

thread_t* create_thread(FuncPtr, const char*, bool); // make this sucka

extern void thread_enter_context(thread_t* from, thread_t* to);
void switch_thread_context(thread_t* from, thread_t* to);
LIGHT_STATUS thread_prepare_context(thread_t*);

void thread_set_state(thread_t*, ThreadState);
LIGHT_STATUS kill_thread(thread_t*); // kill thread and prepare for context swap to kernel
LIGHT_STATUS kill_thread_if(thread_t*, bool); // kill if condition is met
LIGHT_STATUS clean_thread(thread_t*); // clean resources thead used
void exit_thread();

#endif // !__LIGHT_THREAD__

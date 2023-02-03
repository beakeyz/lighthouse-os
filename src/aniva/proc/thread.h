#ifndef __ANIVA_THREAD__
#define __ANIVA_THREAD__

#include "libk/error.h"
#include "mem/PagingComplex.h"
#include "proc/context.h"
#include "system/processor/fpu/state.h"
#include "system/processor/gdt.h"
#include <system/processor/registers.h>
#include <libk/stddef.h>
#include "core.h"

struct proc;
struct threaded_socket;

// TODO: thread uid?
typedef struct thread {
  struct thread* m_self;

  kContext_t m_context;
  FpuState m_fpu_state;

  char m_name[32];
  uint32_t m_cpu; // atomic?
  uint32_t m_ticks_elapsed;
  uint32_t m_max_ticks;

  bool m_has_been_scheduled;
  __attribute__((aligned(16))) uintptr_t m_stack_top;

  thread_state_t m_current_state;

  // allow nested context switches
  struct proc *m_parent_proc; // nullable

  // every thread should have the capability to be a socket,
  // but not every thread should be treated as one
  struct threaded_socket* m_socket;
} thread_t;

thread_t *create_thread(FuncPtr, uintptr_t, char[32], bool); // make this sucka
thread_t *create_thread_for_proc(struct proc *, FuncPtr, uintptr_t, char[32]);
thread_t *create_thread_as_socket(struct proc*, FuncPtr, char[32], uint32_t);
void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t data);

extern void thread_enter_context(thread_t *to);
void bootstrap_thread_entries(thread_t* thread);
void thread_save_context(thread_t* thread);
void thread_switch_context(thread_t* from, thread_t* to);

ANIVA_STATUS thread_prepare_context(thread_t *);

void thread_set_state(thread_t *, thread_state_t);

ANIVA_STATUS kill_thread(thread_t *); // kill thread and prepare for context swap to kernel
ANIVA_STATUS kill_thread_if(thread_t *, bool); // kill if condition is met
ANIVA_STATUS clean_thread(thread_t *); // clean resources thead used
void exit_thread();

// wrappers =D
ALWAYS_INLINE bool thread_is_socket(thread_t* t) {
  return (t->m_socket != nullptr);
}
ALWAYS_INLINE bool thread_has_been_scheduled_before(thread_t* t) {
  return t->m_has_been_scheduled;
}

#endif // !__ANIVA_THREAD__

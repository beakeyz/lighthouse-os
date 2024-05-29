#ifndef __ANIVA_THREAD__
#define __ANIVA_THREAD__

#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "proc/context.h"
#include <sync/atomic_ptr.h>
#include "system/processor/fpu/state.h"
#include <system/processor/registers.h>
#include <libk/stddef.h>
#include "core.h"

struct proc;
struct thread;
struct mutex;
struct threaded_socket;
struct drv_manifest;

typedef int (*ThreadEntry) (
  uintptr_t arg
);

typedef struct thread_rt {
  /* Register context for a thread */
  thread_context_t registers;
  /* FPU/SSE register state */
  FpuState fpu_state;
} thread_rt_t;

typedef struct thread_state_stack {
  THREAD_STATE_t* stack;
  THREAD_STATE_t base_state;
  uint32_t current_state_idx;
  uint32_t stack_capacity;
} __attribute__((packed)) thread_state_stack_t;

int init_thread_state_stack(thread_state_stack_t* stack, uint32_t capacity);
void destroy_thread_state_stack(thread_state_stack_t* stack);

int thread_state_stack_push(thread_state_stack_t* stac, THREAD_STATE_t state);
int thread_state_stack_pop(thread_state_stack_t* stac, THREAD_STATE_t* state);
int thread_state_stack_peek(thread_state_stack_t* stac, THREAD_STATE_t* state);

/*
 *
 */
typedef struct thread {
  const char* m_name;
  /* Pointer to the function which serves as the thread context */
  ThreadEntry f_entry;
  /* (TODO: Use) Exit function to be called when the thread exits */
  FuncPtr f_exit;

  /* Lock that protects the threads internal data */
  struct mutex* m_lock;
  list_t* m_locked_mutexes;

  /* Storage for the thread runtime when switching away from this thread */
  thread_rt_t m_runtime_state;

  struct proc* parent_proc;

  thread_id_t tid;
  uint32_t m_cpu;

  /* Stack to store thread state */
  thread_state_stack_t state;

  uint64_t m_ticks_elapsed;
  uint64_t m_max_ticks;

  /* The vaddress of the stack bottom, as seen by the kernel */
  vaddr_t m_kernel_stack_bottom;
  vaddr_t m_kernel_stack_top;
  /* The vaddress of the stack bottom and top, from the process */
  uintptr_t m_user_stack_bottom;
  uintptr_t m_user_stack_top;

  uintptr_t param0;
} thread_t;

/*
 * create a thread structure
 * when passing NULL to ThreadEntryWrapper, we use the default
 */
thread_t *create_thread(FuncPtr, uintptr_t, const char*, bool); // make this sucka
void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t arg0, uintptr_t arg1);

/*
 * create a thread that is supposed to execute code for a process
 */
thread_t *create_thread_for_proc(struct proc *, FuncPtr, uintptr_t, const char*);

/*
 * set up the thread and prepare to switch context
 * to it for the first time
 * (common_entry -> entry_wrapper -> real_entry)
 */
void bootstrap_thread_entries(thread_t* thread);

/*
 * switch the processor context from one thread to another
 */
void thread_switch_context(thread_t* from, thread_t* to);

/*
 * set up the necessary stuff in order to have a switchable thread
 */
ANIVA_STATUS thread_prepare_context(thread_t *);

void thread_set_state(thread_t* thread, THREAD_STATE_t state);
void thread_get_state(thread_t* thread, THREAD_STATE_t* state);
void thread_push_state(thread_t* thread, THREAD_STATE_t state);
void thread_pop_state(thread_t* thread, THREAD_STATE_t* state);

/*
 * Cleans up any resources used by this thread
 */
ANIVA_STATUS destroy_thread(thread_t *);

/*
 * Returns the default idle thread that can be used for any thread that does not
 * need any special idle behaviour
 */
thread_t* get_generic_idle_thread();

/*
 * TODO: blocking means we get ignored by the scheduler
 * this can happen through multiple means in threory,
 * but for now it contains:
 *  - mutexes
 *
 * we will get unblocked when the mutex is released
 */
void thread_block(thread_t* thread);

/*
 * TODO: unblocking means returning to the runnable threadpool
 */
void thread_unblock(thread_t* thread);

/*
 * TODO:
 */
void thread_sleep(thread_t* thread);

/*
 * TODO:
 */
void thread_wakeup(thread_t* thread);


#define THREAD_PUSH_REGS   \
    "pushfq \n"            \
    "pushq %%r15 \n"       \
    "pushq %%r14 \n"       \
    "pushq %%r13 \n"       \
    "pushq %%r12 \n"       \
    "pushq %%r11 \n"       \
    "pushq %%r10 \n"       \
    "pushq %%r9 \n"        \
    "pushq %%r8 \n"        \
    "pushq %%rax \n"       \
    "pushq %%rbx \n"       \
    "pushq %%rcx \n"       \
    "pushq %%rdx \n"       \
    "pushq %%rbp \n"       \
    "pushq %%rsi \n"       \
    "pushq %%rdi \n"


#define THREAD_POP_REGS    \
    "popq %%rdi \n"        \
    "popq %%rsi \n"        \
    "popq %%rbp \n"        \
    "popq %%rdx \n"        \
    "popq %%rcx \n"        \
    "popq %%rbx \n"        \
    "popq %%rax \n"        \
    "popq %%r8 \n"         \
    "popq %%r9 \n"         \
    "popq %%r10 \n"        \
    "popq %%r11 \n"        \
    "popq %%r12 \n"        \
    "popq %%r13 \n"        \
    "popq %%r14 \n"        \
    "popq %%r15 \n"        \
    "popfq \n"

#endif // !__ANIVA_THREAD__

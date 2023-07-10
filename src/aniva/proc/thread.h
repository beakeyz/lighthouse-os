#ifndef __ANIVA_THREAD__
#define __ANIVA_THREAD__

#include "libk/flow/error.h"
#include "mem/pg.h"
#include "proc/context.h"
#include "proc/socket.h"
#include <sync/atomic_ptr.h>
#include "system/processor/fpu/state.h"
#include "system/processor/gdt.h"
#include <system/processor/registers.h>
#include <libk/stddef.h>
#include "core.h"

struct proc;
struct thread;
struct mutex;
struct threaded_socket;
struct aniva_driver;

typedef int (*ThreadEntry) (
  uintptr_t arg
);

typedef void (*ThreadEntryWrapper) (
  uintptr_t args,
  struct thread* thread
);


// TODO: thread uid?
typedef struct thread {
  struct thread* m_self;

  ThreadEntry f_real_entry;
  ThreadEntryWrapper f_entry_wrapper;
  FuncPtr f_exit;

  struct mutex* m_lock;
  struct aniva_driver* m_qdriver; /* The current querying driver */

  kContext_t m_context;
  FpuState m_fpu_state;

  char m_name[32];
  thread_id_t m_tid;
  uint32_t m_cpu; // atomic?
  uint32_t m_ticks_elapsed;
  uint32_t m_max_ticks;

  bool m_has_been_scheduled;
  /* The vaddress of the stack bottom, as seen by the kernel */
  vaddr_t m_kernel_stack_bottom;
  vaddr_t m_kernel_stack_top;
  /* The vaddress of the stack bottom and top, from the process */
  uintptr_t m_user_stack_bottom;
  uintptr_t m_user_stack_top;

  thread_state_t m_current_state;

  // allow nested context switches
  struct proc *m_parent_proc; // nullable?

  /* The virtual address of the relocated entry. nullptr if there isn't one */
  void (*f_relocated_entry_stub) (FuncPtr real_entry);

  // every thread should have the capability to be a socket,
  // but not every thread should be treated as one
  struct threaded_socket* m_socket;
} thread_t;

/*
 * create a thread structure
 * when passing NULL to ThreadEntryWrapper, we use the default
 */
thread_t *create_thread(FuncPtr, ThreadEntryWrapper, uintptr_t, const char[32], struct proc*, bool); // make this sucka

/*
 * create a thread that is supposed to execute code for a process
 */
thread_t *create_thread_for_proc(struct proc *, FuncPtr, uintptr_t, const char[32]);

/*
 * create a thread that is supposed to act as a socket for itc and ipc
 * NOTE: this mutates the port value to something which is always available
 */
thread_t *create_thread_as_socket(struct proc* process, FuncPtr entry, uintptr_t arg0, FuncPtr exit_fn, SocketOnPacket on_packet_fn, char name[32], uint32_t* port);

void thread_set_qdrv(thread_t* t, struct aniva_driver* driver);

struct aniva_driver* thread_get_qdrv(thread_t* t);

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

/*
 * set the current state of a thread
 */
void thread_set_state(thread_t *, thread_state_t);

/*
 * Cleans up any resources used by this thread
 */
ANIVA_STATUS destroy_thread(thread_t *);

/*
 * Returns the default idle thread that can be used for any thread that does not
 * need any special idle behaviour
 */
thread_t* get_generic_idle_thread();

#define T_SOCKET(t) (t->m_socket)

// wrappers =D
ALWAYS_INLINE bool thread_is_socket(thread_t* t) {
  return (t->m_socket != nullptr);
}
ALWAYS_INLINE bool thread_has_been_scheduled_before(thread_t* t) {
  return t->m_has_been_scheduled;
}

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

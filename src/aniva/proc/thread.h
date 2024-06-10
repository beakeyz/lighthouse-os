#ifndef __ANIVA_THREAD__
#define __ANIVA_THREAD__

#include "core.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/syscall.h"
#include "proc/context.h"
#include "system/processor/fpu/state.h"
#include <libk/stddef.h>
#include <sync/atomic_ptr.h>
#include <system/processor/registers.h>

struct proc;
struct thread;
struct mutex;
struct threaded_socket;
struct drv_manifest;

typedef int (*ThreadEntry)(
    uintptr_t arg);

#define THREAD_FLAGS_HAS_RAN 0x00000001
#define THREAD_FLAGS_SYSCALLING 0x00000002

// TODO: thread uid?
typedef struct thread {
    const char* m_name;
    struct mutex* m_lock;
    struct proc* m_parent_proc;
    /* TODO: figure out if this is legal */
    list_t* m_mutex_list;
    ThreadEntry f_entry;

    thread_context_t m_context;
    uint64_t m_ticks_elapsed;
    uint64_t m_max_ticks;
    FpuState m_fpu_state;

    thread_id_t m_tid;
    uint32_t m_cpu; // atomic?
    enum SYSID m_c_sysid;
    THREAD_STATE_t m_current_state;

    /* The vaddress of the stack bottom, as seen by the kernel */
    vaddr_t m_kernel_stack_bottom;
    vaddr_t m_kernel_stack_top;
    /* The vaddress of the stack bottom and top, from the process */
    uintptr_t m_user_stack_bottom;
    uintptr_t m_user_stack_top;
} ALIGN(0x400) thread_t;

/*
 * create a thread structure
 * when passing NULL to ThreadEntryWrapper, we use the default
 */
thread_t* create_thread(FuncPtr, uintptr_t, const char*, struct proc*, bool); // make this sucka

/*
 * Cleans up any resources used by this thread
 */
ANIVA_STATUS destroy_thread(thread_t*);

void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t arg0, uintptr_t arg1);

static inline bool thread_is_runnable(thread_t* t)
{
    return (t->m_current_state == RUNNABLE);
}

/*
 * create a thread that is supposed to execute code for a process
 */
thread_t* create_thread_for_proc(struct proc*, FuncPtr, uintptr_t, const char[32]);

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
ANIVA_STATUS thread_prepare_context(thread_t*);

/*
 * set the current state of a thread
 */
void thread_set_state(thread_t*, THREAD_STATE_t);

/*
 * Returns the default idle thread that can be used for any thread that does not
 * need any special idle behaviour
 */
thread_t* get_generic_idle_thread();

bool thread_try_revert_userpacket_context(registers_t* regs, thread_t* thread);
void thread_try_prepare_userpacket(thread_t* to);

void thread_register_mutex(thread_t* thread, struct mutex* lock);
void thread_unregister_mutex(thread_t* thread, struct mutex* lock);

void thread_disable_scheduling(thread_t* thread);
void thread_set_max_ticks(thread_t* thread, uintptr_t max_ticks);
ssize_t thread_ticksleft(thread_t* thread);

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

#define THREAD_PUSH_REGS \
    "pushfq \n"          \
    "pushq %%r15 \n"     \
    "pushq %%r14 \n"     \
    "pushq %%r13 \n"     \
    "pushq %%r12 \n"     \
    "pushq %%r11 \n"     \
    "pushq %%r10 \n"     \
    "pushq %%r9 \n"      \
    "pushq %%r8 \n"      \
    "pushq %%rax \n"     \
    "pushq %%rbx \n"     \
    "pushq %%rcx \n"     \
    "pushq %%rdx \n"     \
    "pushq %%rbp \n"     \
    "pushq %%rsi \n"     \
    "pushq %%rdi \n"

#define THREAD_POP_REGS \
    "popq %%rdi \n"     \
    "popq %%rsi \n"     \
    "popq %%rbp \n"     \
    "popq %%rdx \n"     \
    "popq %%rcx \n"     \
    "popq %%rbx \n"     \
    "popq %%rax \n"     \
    "popq %%r8 \n"      \
    "popq %%r9 \n"      \
    "popq %%r10 \n"     \
    "popq %%r11 \n"     \
    "popq %%r12 \n"     \
    "popq %%r13 \n"     \
    "popq %%r14 \n"     \
    "popq %%r15 \n"     \
    "popfq \n"

#endif // !__ANIVA_THREAD__

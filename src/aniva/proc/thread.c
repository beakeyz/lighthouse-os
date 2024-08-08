#include "thread.h"
#include "core.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/stack.h"
#include "lightos/syscall.h"
#include "mem/kmem_manager.h"
#include "proc/context.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"
#include <libk/string.h>
#include <mem/heap.h>

extern void first_ctx_init(thread_t* from, thread_t* to, registers_t* regs) __attribute__((used));
extern void thread_exit_init_state(thread_t* from, registers_t* regs) __attribute__((used));
/* Routine that ends our thread */
extern NORETURN void thread_end_lifecycle();

static __attribute__((naked)) void common_thread_entry(void) __attribute__((used));

static thread_t* __generic_idle_thread;

/*
 * Fills the userstack with certain data for userspace
 * and a trampoline so we can return from userspace when
 * the process exits (aka returns)
 */
// static kerror_t __thread_populate_user_stack(thread_t* thread);

thread_t* create_thread(FuncPtr entry, uintptr_t data, const char* name, proc_t* proc, bool kthread)
{ // make this sucka
    thread_t* thread;
    void* k_stack_addr;

    if (!proc)
        proc = get_current_proc();

    thread = allocate_thread();

    if (!thread)
        return nullptr;

    memset(thread, 0, sizeof(thread_t));

    thread->m_name = strdup(name);
    thread->m_lock = create_mutex(0);

    thread->m_cpu = get_current_processor()->m_cpu_num;
    thread->m_c_sysid = SYSID_INVAL | SYSID_INVAL_MASK;
    thread->m_parent_proc = proc;
    thread->m_ticks_elapsed = 0;
    thread->m_max_ticks = DEFAULT_THREAD_MAX_TICKS;
    thread->m_tid = -1;
    thread->m_mutex_list = init_list();

    thread->f_entry = (f_tentry_t)entry;

    /* TODO: thread locking */
    thread->m_tid = proc->m_thread_count;
    proc->m_thread_count = thread->m_tid + 1;

    memcpy(&thread->m_fpu_state, &standard_fpu_state, sizeof(FpuState));

    /* Allocate kernel memory for the stack */
    ASSERT(__kmem_alloc_range(
               (void**)&thread->m_kernel_stack_bottom,
               proc->m_root_pd.m_root,
               proc->m_resource_bundle,
               KERNEL_MAP_BASE,
               DEFAULT_STACK_SIZE,
               NULL,
               KMEM_FLAG_WRITABLE)
        == 0);

    /* Compute the kernel stack top */
    thread->m_kernel_stack_top = ALIGN_DOWN(thread->m_kernel_stack_bottom + DEFAULT_STACK_SIZE - 8, 16);
    thread->m_user_stack_top = 0;
    thread->m_user_stack_bottom = 0;

    /* Zero memory, since we don't want random shit in our stack */
    memset((void*)thread->m_kernel_stack_bottom, 0x00, DEFAULT_STACK_SIZE);

    /* Do context before we assign the userstack */
    thread->m_context = setup_regs(kthread, (pml_entry_t*)proc->m_root_pd.m_phys_root, thread->m_kernel_stack_top);

    /*
     * FIXME: right now, we try to remap the stack every time a thread is created,
     * this means that the stack of the previous thread (if there is one) of this
     * process gets corrupted, so we either need to give every thread its own stack,
     * or we try to somehow let every thread share one stack (which like how tf would that work lol)
     */
    if (!kthread) {
        thread->m_user_stack_bottom = HIGH_STACK_BASE - (thread->m_tid * DEFAULT_STACK_SIZE);

        ASSERT(__kmem_alloc_range(
                   (void**)&thread->m_user_stack_bottom,
                   proc->m_root_pd.m_root,
                   proc->m_resource_bundle,
                   thread->m_user_stack_bottom,
                   DEFAULT_STACK_SIZE,
                   KMEM_CUSTOMFLAG_NO_REMAP | KMEM_CUSTOMFLAG_CREATE_USER,
                   KMEM_FLAG_WRITABLE)
            == 0);

        /* TODO: subtract random offset */
        thread->m_user_stack_top = ALIGN_DOWN(thread->m_user_stack_bottom + DEFAULT_STACK_SIZE - 8, 16);

        ASSERT(kmem_get_kernel_address((uintptr_t*)&k_stack_addr, thread->m_user_stack_bottom, proc->m_root_pd.m_root) == 0);

        /* Clear the user stack */
        memset(k_stack_addr, 0, DEFAULT_STACK_SIZE);

        /* We don't touch rsp when the thread is not a kthread */
        thread->m_context.rsp = thread->m_user_stack_top;
    }

    thread_set_state(thread, NO_CONTEXT);
    /* Set the entrypoint last */
    thread_set_entrypoint(thread, (FuncPtr)thread->f_entry, data, 0);
    return thread;
}

thread_t* create_thread_for_proc(proc_t* proc, FuncPtr entry, uintptr_t args, const char name[32])
{
    if (proc == nullptr)
        return nullptr;

    const bool is_kernel = ((proc->m_flags & PROC_KERNEL) == PROC_KERNEL) || ((proc->m_flags & PROC_DRIVER) == PROC_DRIVER);

    return create_thread(entry, args, name, proc, is_kernel);
}

/*!
 * @brief: Set the
 */
void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t arg0, uintptr_t arg1)
{
    if (ptr->m_current_state != NO_CONTEXT)
        return;

    contex_set_rip(&ptr->m_context, (uintptr_t)entry, arg0, arg1);
}

void thread_set_state(thread_t* thread, THREAD_STATE_t state)
{
    mutex_lock(thread->m_lock);

    thread->m_current_state = state;

    mutex_unlock(thread->m_lock);
}

// TODO: finish
// TODO: when this thread has gotten it's own heap, free that aswell
ANIVA_STATUS destroy_thread(thread_t* thread)
{
    list_t* mutex_list;
    proc_t* parent_proc;

    if (!thread)
        return ANIVA_FAIL;

    /* Cache the mutex list pointer */
    mutex_list = thread->m_mutex_list;

    /* Kill the point so registers don't go through from this point */
    thread->m_mutex_list = nullptr;

    parent_proc = thread->m_parent_proc;

    FOREACH(i, mutex_list)
    {
        /* Drain every taken mutex */
        while (mutex_is_locked(i->data))
            mutex_release(i->data, thread);
    }

    /* Get rid of the mutex list */
    destroy_list(mutex_list);

    __kmem_dealloc(parent_proc->m_root_pd.m_root, parent_proc->m_resource_bundle, thread->m_kernel_stack_bottom, DEFAULT_STACK_SIZE);

    if (thread->m_user_stack_bottom)
        __kmem_dealloc(parent_proc->m_root_pd.m_root, parent_proc->m_resource_bundle, thread->m_user_stack_bottom, DEFAULT_STACK_SIZE);

    destroy_mutex(thread->m_lock);
    kfree((void*)thread->m_name);
    deallocate_thread(thread);
    return ANIVA_SUCCESS;
}

/* FIXME: thread-safe? */
void thread_register_mutex(thread_t* thread, mutex_t* lock)
{
    /* If we can't schedule this process, don't count the register */
    if (!thread || !thread->m_mutex_list)
        return;

    list_append(thread->m_mutex_list, lock);
}

/* FIXME: thread-safe? */
void thread_unregister_mutex(thread_t* thread, mutex_t* lock)
{
    if (!thread || !thread->m_mutex_list)
        return;

    list_remove_ex(thread->m_mutex_list, lock);
}

void thread_set_max_ticks(thread_t* thread, uintptr_t max_ticks)
{
    thread->m_max_ticks = max_ticks;
    thread->m_ticks_elapsed = 0;
}

ssize_t thread_ticksleft(thread_t* thread)
{
    return thread->m_max_ticks - thread->m_ticks_elapsed;
}

thread_t* get_generic_idle_thread()
{
    ASSERT_MSG(__generic_idle_thread, "Tried to get generic idle thread before it was initialized");
    return __generic_idle_thread;
}

/*
 * Routine to be called when a thread ends execution. We can just stick this to the
 * end of the stack of the thread
 * FIXME: when a user process ends this gets entered in usermode. That kinda stinks :clown:
 */
extern NORETURN void thread_end_lifecycle()
{
    // TODO: report or cache the result somewhere until
    // It is approved by the kernel

    kernel_panic("thread_end_lifecycle");

    thread_t* current_thread = get_current_scheduling_thread();

    ASSERT_MSG(current_thread, "Can't end a null thread!");

    thread_set_state(current_thread, DYING);

    // yield will enable interrupts again (I hope)
    scheduler_yield();

    kernel_panic("thread_end_lifecycle returned!");
}

NAKED void common_thread_entry()
{
    asm volatile(
        "popq %rdi \n" // our beautiful thread
        "popq %rsi \n" // ptr to its registers
        //"call thread_exit_init_state \n" // Call this to get bread
        // Go to the portal that might just take us to userland
        "jmp asm_common_irq_exit \n");
}

/*!
 * @brief: Called every context switch
 */
extern void thread_enter_context(thread_t* to)
{
    processor_t* cur_cpu;
    thread_t* prev_thread;

    /* Check that we are legal */
    ASSERT_MSG(thread_is_runnable(to), to_string(to->m_current_state));

    cur_cpu = get_current_processor();
    prev_thread = get_previous_scheduled_thread();

    // NOTE: for correction purposes
    to->m_cpu = cur_cpu->m_cpu_num;

    /* Gib floats */
    load_fpu_state(&to->m_fpu_state);

    thread_set_state(to, RUNNING);

    // Only switch pagetables if we actually need to interchange between
    // them, otherwise thats just wasted tlb
    if (prev_thread->m_context.cr3 == to->m_context.cr3)
        return;

    /*
    printf("Switching from %s:%s (%d-%d) (%lld) to %s:%s (%d-%d) (%lld)\n",
        prev_thread->m_parent_proc->m_name,
        prev_thread->m_name,
        prev_thread->m_parent_proc->m_id,
        prev_thread->m_tid,
        prev_thread->m_context.cr3,
        to->m_parent_proc->m_name,
        to->m_name,
        to->m_parent_proc->m_id,
        to->m_tid,
        to->m_context.cr3
        );
        */

    kmem_load_page_dir(to->m_context.cr3, false);
}

/*!
 * @brief: Called when a thread is created and is added to the scheduler for the first time
 */
ANIVA_STATUS thread_prepare_context(thread_t* thread)
{
    uintptr_t rsp = thread->m_kernel_stack_top;

    if (thread->m_current_state != NO_CONTEXT)
        return ANIVA_FAIL;

    /*
     * Stitch the exit function at the end of the thread stack
     * NOTE: we can do this since STACK_PUSH first decrements the
     *       stack pointer and then it places the value
     * TODO: Put an actual thing here?
     */
    *(uintptr_t*)rsp = (uintptr_t)NULL;

    if ((thread->m_context.cs & 3) != 0) {
        STACK_PUSH(rsp, uintptr_t, GDT_USER_DATA | 3);
        STACK_PUSH(rsp, uintptr_t, thread->m_context.rsp);
    } else {
        STACK_PUSH(rsp, uintptr_t, 0);
        STACK_PUSH(rsp, uintptr_t, thread->m_kernel_stack_top);
    }

    STACK_PUSH(rsp, uintptr_t, thread->m_context.rflags);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.cs);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rip);

    // dummy irs_num and err_code
    STACK_PUSH(rsp, uintptr_t, 0);
    STACK_PUSH(rsp, uintptr_t, 0);

    STACK_PUSH(rsp, uintptr_t, thread->m_context.r15);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.r14);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.r13);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.r12);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.r11);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.r10);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.r9);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.r8);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rax);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rbx);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rcx);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rdx);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rbp);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rsi);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rdi);

    // ptr to registers_t struct above
    STACK_PUSH(rsp, uintptr_t, rsp + 8);

    thread->m_context.rip = (uintptr_t)&common_thread_entry;
    thread->m_context.rsp = rsp;
    thread->m_context.rsp0 = thread->m_kernel_stack_top;
    thread->m_context.cs = GDT_KERNEL_CODE;

    thread_set_state(thread, RUNNABLE);
    return ANIVA_SUCCESS;
}

// used to bootstrap the iret stub created in thread_prepare_context
// only on the first context switch
void bootstrap_thread_entries(thread_t* thread)
{
    /* Prepare that bitch */
    if (thread->m_current_state == NO_CONTEXT)
        thread_prepare_context(thread);

    ASSERT(get_current_scheduling_thread() == thread);
    thread_set_state(thread, RUNNING);

    tss_entry_t* tss_ptr = &get_current_processor()->m_tss;
    tss_ptr->iomap_base = sizeof(get_current_processor()->m_tss);
    tss_ptr->rsp0l = thread->m_context.rsp0 & 0xffffffff;
    tss_ptr->rsp0h = (thread->m_context.rsp0 >> 32) & 0xffffffff;

    asm volatile(
        "movq %[new_rsp], %%rsp \n"
        "pushq %[thread] \n"
        "pushq %[new_rip] \n"
        "movq 16(%%rsp), %%rdi \n"
        "movq $0, %%rsi \n"
        "call processor_enter_interruption \n"
        "retq \n"
        : "=d"(thread)
        :
        [new_rsp] "g"(thread->m_context.rsp),
        [new_rip] "g"(thread->m_context.rip),
        [thread] "d"(thread)
        : "memory");
}

/*
 * To be called from a faulthandler directly out of the userpacket context
 * which means interrupts are off
 */
bool thread_try_revert_userpacket_context(registers_t* regs, thread_t* thread)
{
    return false;
}

void thread_try_prepare_userpacket(thread_t* to)
{
}

void thread_switch_context(thread_t* from, thread_t* to)
{

    // FIXME: saving context is still wonky!!!
    // without saving the context it kinda works,
    // but it keeps running the function all over again.
    // since no state is saved, is just starts all over
    ASSERT_MSG(from, "Switched from NULL thread!");

    if (from->m_current_state == RUNNING)
        thread_set_state(from, RUNNABLE);

    ASSERT_MSG(get_previous_scheduled_thread() == from, "get_previous_scheduled_thread() is not the thread we came from!");

    tss_entry_t* tss_ptr = &get_current_processor()->m_tss;

    save_fpu_state(&from->m_fpu_state);

    asm volatile(
        THREAD_PUSH_REGS
        // save rsp
        "movq %%rsp, %[old_rsp] \n"
        // save rip
        "leaq 1f(%%rip), %%rbx \n"
        "movq %%rbx, %[old_rip] \n"
        // restore tss
        "movq %[base_stack_top], %%rbx \n"
        "movl %%ebx, %[tss_rsp0l] \n"
        "shrq $32, %%rbx \n"
        "movl %%ebx, %[tss_rsp0h] \n"

        "movq %[new_rsp], %%rsp \n"
        "movq %%rbp, %[old_rbp] \n"
        "pushq %[thread] \n"
        "pushq %[new_rip] \n"

        // prepare for switch
        "cld \n"
        "movq 8(%%rsp), %%rdi \n"
        "jmp thread_enter_context \n"
        // pick up where we left of
        "1: \n"
        "cli \n"
        "addq $8, %%rsp \n" THREAD_POP_REGS
        :
        [old_rbp] "=m"(from->m_context.rbp),
        [old_rip] "=m"(from->m_context.rip),
        [old_rsp] "=m"(from->m_context.rsp),
        [tss_rsp0l] "=m"(tss_ptr->rsp0l),
        [tss_rsp0h] "=m"(tss_ptr->rsp0h),
        "=d"(to)
        :
        [new_rsp] "g"(to->m_context.rsp),
        [new_rip] "g"(to->m_context.rip),
        [base_stack_top] "g"(to->m_context.rsp0),
        [thread] "d"(to)
        : "memory", "rbx");
}

/*!
 * @brief: (NOTE: Not used atm) Called once right before we jump to the thread entry
 *
 */
void thread_exit_init_state(thread_t* from, registers_t* regs)
{
}

void thread_block(thread_t* thread)
{
    thread_set_state(thread, BLOCKED);

    // FIXME: we do allow blocking other threads here, we need to
    // check if that comes with extra complications
    if (thread == get_current_scheduling_thread())
        scheduler_yield();
}

/*
 * TODO: unblocking means returning to the runnable threadpool
 */
void thread_unblock(thread_t* thread)
{
    //ASSERT_MSG(thread->m_current_state == BLOCKED, "Tried to unblock a non-blocking thread!");

    if (thread->m_current_state != BLOCKED)
        return;

    /* We can't unblock ourselves, can we? */
    if (get_current_scheduling_thread() == thread) {
        thread_set_state(thread, RUNNING);
        kernel_panic("Wait, how did we unblock ourselves?");
        return;
    }

    thread_set_state(thread, RUNNABLE);
}

void thread_sleep(thread_t* thread)
{
    thread_set_state(thread, SLEEPING);

    // FIXME: we do allow blocking other threads here, we need to
    // check if that comes with extra complications
    if (thread == get_current_scheduling_thread())
        scheduler_yield();
}

void thread_wakeup(thread_t* thread)
{
    ASSERT_MSG(thread->m_current_state == SLEEPING, "Tried to wake a non-sleeping thread!");

    /* We can't unblock ourselves, can we? */
    if (get_current_scheduling_thread() == thread) {
        thread_set_state(thread, RUNNING);
        kernel_panic("Wait, how did we wake ourselves?");
        return;
    }

    thread_set_state(thread, RUNNABLE);
}

void thread_stop(thread_t* thread)
{
    thread_t* c_thread;

    if (!thread)
        return;

    c_thread = get_current_scheduling_thread();

    thread_set_max_ticks(thread, 0);
    thread_set_state(thread, STOPPED);

    /* Yield to get out of the scheduler */
    if (thread == c_thread)
        scheduler_yield();
}

#include "scheduler.h"
#include "entry/entry.h"
#include "irq/interrupts.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/zalloc/zalloc.h"
#include "proc/kprocs/idle.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/time.h"
#include "sync/spinlock.h"
#include "sys/types.h"
#include "system/asm_specifics.h"
#include "system/processor/processor.h"
#include "time/core.h"
#include <mem/heap.h>

// --- inline functions ---
static inline void scheduler_do_tick(scheduler_t* sched, u64 tick_delta, bool force);
static ALWAYS_INLINE void set_previous_thread(thread_t* thread);
static ALWAYS_INLINE void set_current_proc(proc_t* proc);

/* Default scheduler tick function */
static void sched_tick(scheduler_t* s, registers_t* registers_ptr, u64 timeticks);

/*!
 * @brief: Helper to easily track interrupt disables and disable
 */
static inline void scheduler_try_disable_interrupts(scheduler_t* s)
{
    ASSERT(s);

    /* If interrupts where already disabled, ensure we don't accedentally enable them */
    if (!interrupts_are_enabled() && !s->int_disable_depth)
        s->int_disable_depth++;

    disable_interrupts();

    s->int_disable_depth++;
}

/*!
 * @brief: Helper to easily track interrupt enables and enable
 */
static inline void scheduler_try_enable_interrupts(scheduler_t* s)
{
    ASSERT(s);

    if (s->int_disable_depth)
        s->int_disable_depth--;

    if (!s->int_disable_depth)
        enable_interrupts();
}

/*!
 * @brief: Sets the current thread for the current processor
 * doing this means we have two pointers that point to the
 * same memory address (thread) idk if this might get ugly
 * TODO: sync?
 */
void set_current_handled_thread(thread_t* thread)
{
    scheduler_t* s;

    s = get_current_scheduler();

    if (!s)
        return;

    scheduler_try_disable_interrupts(s);

    processor_t* current_processor = get_current_processor();
    current_processor->m_current_thread = thread;

    scheduler_try_enable_interrupts(s);
}

/*!
 * @brief: Sets the current proc for the current processor
 */
static ALWAYS_INLINE void set_current_proc(proc_t* proc)
{
    scheduler_t* s;

    s = get_current_scheduler();

    if (!s)
        return;

    scheduler_try_disable_interrupts(s);

    processor_t* current_processor = get_current_processor();
    current_processor->m_current_proc = proc;

    scheduler_try_enable_interrupts(s);
}

ALWAYS_INLINE void set_previous_thread(thread_t* thread)
{
    scheduler_t* s;
    processor_t* current_processor;

    s = get_current_scheduler();

    if (!s)
        return;

    current_processor = get_current_processor();

    scheduler_try_disable_interrupts(s);

    current_processor->m_previous_thread = thread;

    scheduler_try_enable_interrupts(s);
}

thread_t* get_current_scheduling_thread()
{
    return (thread_t*)read_gs(GET_OFFSET(processor_t, m_current_thread));
}

thread_t* get_previous_scheduled_thread()
{
    return (thread_t*)read_gs(GET_OFFSET(processor_t, m_previous_thread));
}

proc_t* get_current_proc()
{
    return (proc_t*)read_gs(GET_OFFSET(processor_t, m_current_proc));
}

proc_t* sched_get_kernel_proc()
{
    return (proc_t*)read_gs(GET_OFFSET(processor_t, m_kernel_process));
}

scheduler_t* get_current_scheduler()
{
    return (scheduler_t*)read_gs(GET_OFFSET(processor_t, m_scheduler));
}

kerror_t set_kernel_proc(proc_t* proc)
{
    processor_t* current = get_current_processor();

    if (current->m_kernel_process != nullptr)
        return -1;

    current->m_kernel_process = proc;
    return 0;
}

/*!
 * @brief: Initialize a scheduler on cpu @cpu
 *
 * This also attaches the scheduler to said processor
 * and from here on out, the processor owns scheduler memory
 */
ANIVA_STATUS init_scheduler(uint32_t cpu)
{
    scheduler_t* this;
    processor_t* processor;

    /* Make sure we don't get fucked */
    disable_interrupts();

    processor = processor_get(cpu);

    /* Can't init a processor with an active scheduler */
    if (!processor || processor->m_scheduler)
        return ANIVA_FAIL;

    processor->m_scheduler = kmalloc(sizeof(*this));

    if (!processor->m_scheduler)
        return ANIVA_FAIL;

    this = processor->m_scheduler;

    memset(this, 0, sizeof(*this));

    this->lock = create_spinlock(0);
    this->sthread_allocator = create_zone_allocator(64 * Kib, sizeof(sthread_t), NULL);
    this->idle = create_sthread(this, __kernel_idle_thread, SCHED_PRIO_LOWEST);
    this->f_tick = sched_tick;

    this->idle->tslice = 0;

    /* Grab the scheduler base time */
    time_get_system_time(&this->scheduler_base_time);

    /* Initialize both scheduler thread queues */
    init_squeue(&this->queues[0]);
    init_squeue(&this->queues[1]);

    /* Initialize the queue pointers */
    this->active_q = &this->queues[0];
    this->expired_q = &this->queues[1];

    set_current_handled_thread(nullptr);
    set_current_proc(nullptr);

    return ANIVA_SUCCESS;
}

/*!
 * @brief: Quick helper to get the scheduler of a processor
 */
scheduler_t* get_scheduler_for(uint32_t cpu)
{
    processor_t* p;

    p = processor_get(cpu);

    if (!p)
        return nullptr;

    return p->m_scheduler;
}

/*!
 * @brief: Starts up the scheduler on the current CPU
 *
 * Should not return really, so TODO: make this panic on failure
 */
void start_scheduler(void)
{
    scheduler_t* sched;
    thread_t* initial_thread;

    sched = get_current_scheduler();

    if (!sched || !sched->idle)
        return;

    // let's jump into the idle thread initially, so we can then
    // resume to the thread-pool when we leave critical sections
    initial_thread = sched->idle->t;

    if (!initial_thread)
        return;

    /* Set some processor state */
    set_current_proc(initial_thread->m_parent_proc);
    set_current_handled_thread(initial_thread);
    set_previous_thread(nullptr);

    sched->flags |= SCHEDULER_FLAG_RUNNING;
    g_system_info.sys_flags |= SYSFLAGS_HAS_MULTITHREADING;

    bootstrap_thread_entries(initial_thread);
}

/*!
 * @brief: Pause the scheduler from scheduling
 *
 * This disables interrupts for a moment to make sure we don't get
 * interrupted right before we actually pause the scheduler
 *
 * When we're already paused, we just increment s_sched_pause_depth and
 * return
 *
 * Pausing the scheduler means that someone/something wants to edit the scheduler
 * context (adding/removing processes, threads, ect.) and for that it needs to know
 * for sure the scheduler does not decide to suddenly remove it from existence
 */
ANIVA_STATUS pause_scheduler()
{
    scheduler_t* s;

    s = get_current_scheduler();

    if (!s)
        return ANIVA_FAIL;

    /* If we are trying to pause inside of a pause, just increment the pause depth */
    if (s->pause_depth)
        goto skip_lock;

    spinlock_lock(s->lock);

    s->flags |= SCHEDULER_FLAG_PAUSED;

skip_lock:
    s->pause_depth++;

    return ANIVA_SUCCESS;
}

/*!
 * @brief: Unpause the scheduler from scheduling
 *
 * This disables interrupts for a moment to make sure we don't get
 * interrupted right before we actually resume the scheduler
 *
 * Only sets the scheduler back to SCHEDULING once we've reached a
 * s_sched_pause_depth of 0
 */
ANIVA_STATUS resume_scheduler()
{
    scheduler_t* s;

    s = get_current_scheduler();

    if (!s)
        return ANIVA_FAIL;

    /* Simply not paused /-/ */
    if (!s->pause_depth)
        return ANIVA_FAIL;

    /* Only decrement if we can */
    if (s->pause_depth)
        s->pause_depth--;

    /* Only unlock if we just decremented to the last pause level =) */
    if (!s->pause_depth) {
        // make sure the scheduler is in the right state
        s->flags &= ~SCHEDULER_FLAG_PAUSED;

        /* Unlock the spinlock */
        spinlock_unlock(s->lock);
    }

    return ANIVA_SUCCESS;
}

static inline void scheduler_move_thread_to_inactive(scheduler_t* scheduler, sthread_t** active_thread)
{
    sthread_t* p_thread;

    /* Grab the direct pointer */
    p_thread = *active_thread;

    // KLOG_DBG("Moving %s to inactive. New tslice: %d\n", p_thread->t->m_name, p_thread->tslice);

    /* Calculate the new timeslice for this thread */
    sthread_calc_stimeslice(p_thread);

    /* Remove it from the active queue and add it to the inactive queue */
    squeue_remove(scheduler->active_q, active_thread);
    squeue_enqueue(scheduler->expired_q, p_thread);
}

/*!
 * @brief: Requeues the active scheduler thread in the active scheduler queue
 *
 * If the scheduler thread still has timeslice left, it simply advances the queue. Otherwise
 *
 * @active_thread: The active thread we want to requeue
 */
static inline void scheduler_do_requeue(scheduler_t* scheduler, sthread_t** active_thread)
{
    sthread_t* sthread;

    /* Just to be sure */
    if (!active_thread || !(*active_thread))
        return;

    sthread = *active_thread;
    // KLOG_DBG("Doing requeue for %s:%s\n", (*active_thread)->t->m_parent_proc->m_name, (*active_thread)->t->m_name);

    /* Does this thread still have time in this epoch? */
    if (sthread->c_queue && sthread->tslice <= 0)
        /* Thread has used up it's timeslice, recalculate it and move the thread to expired */
        scheduler_move_thread_to_inactive(scheduler, active_thread);
    else {
        sthread->tslice -= sthread->elapsed_tslice;
        sthread->elapsed_tslice = 0;
    }
}

/*!
 * @brief: Ends a single scheduler epoch
 *
 * At the end of an epoch, we need to switch around the two scheduler queues, in order to
 * continue scheduling as normal. We assume that at this point the active (soon to be inactive)
 * queue is empty and the inactive queue contains all threads of this scheduler.
 */
static inline void scheduler_end_epoch(scheduler_t* scheduler, sthread_t** pSThread)
{
    // KLOG_DBG("Scheduler: Ending epoch!\n");
    if (!scheduler->expired_q->n_sthread)
        return;

    /* Switch the two queue pointers around, if the inactive_q does have threads */
    scheduler_switch_queues(scheduler);

    /* Make sure the active thread is reset propperly */
    scheduler->active_q->active_prio = SCHED_PRIO_7;

    /* Try to find the correct active prio in the active scheduler queue */
    while (scheduler->active_q->active_prio && !squeue_get_active_threadlist(scheduler->active_q))
        scheduler->active_q->active_prio--;

    /* Export the current active thread */
    if (pSThread)
        *pSThread = squeue_get_active_threadlist(scheduler->active_q);
}

/*!
 * @brief: Try to execute a schedule on the current scheduler
 *
 * Checks if the current thread needs to get rescheduled
 * TODO: This function seems to be quite heavy and gets called quite often. We need to check
 * if we can simplify this to make it faster.
 */
int scheduler_try_execute(struct processor* p)
{
    scheduler_t* s;
    thread_t* c_thread;
    sthread_t* target_thread;

    s = get_current_scheduler();

    ASSERT_MSG(p && s, "Could not get current processor while trying to calling scheduler");
    ASSERT_MSG(p->m_irq_depth == 0, "Trying to call scheduler while in irq");

    if (s->pause_depth)
        return -KERR_INVAL;

    /*
     * Grab the current thread
     * NOTE: This thread might not have a scheduler thread at this point, due to possible removal from one
     * of the queues. This is okay, since we only do requeues when we have a slot and otherwise we can just
     * switch to a next thread anyway
     */
    c_thread = get_current_scheduling_thread();

    if (!c_thread)
        return -KERR_INVAL;

    // KLOG_DBG("%s: tslice: %d, elapsed: %d\n", c_thread->m_name, c_thread->sthread->tslice, c_thread->sthread->elapsed_tslice);

    /* We need to requeue the current sthread lolol */
    if (!sthread_needs_requeue(c_thread->sthread))
        return KERR_HANDLED;

    /* Do the requeue */
    scheduler_do_requeue(s, c_thread->sthread_slot);

    /* Clear target thread */
    target_thread = NULL;

    /*
     * If there are no threads left in the active queue, we need to end this epoch,
     * which switches the expired and active queues and resets the active priority
     */
    if (!s->active_q->n_sthread)
        scheduler_end_epoch(s, &target_thread);

    if (!target_thread || !target_thread->c_queue)
        /* Grab the current target thread */
        target_thread = scheduler_get_new_thread(s, c_thread->sthread);

    // KLOG_DBG("Active thread: %d, Inactive threads: %d\n", s->active_q->n_sthread, s->expired_q->n_sthread);

    /* Actually no threads, switch to the idle thread */
    if (!target_thread) {
        target_thread = s->idle;

        KLOG_INFO("Scheduler: Switching to idle thread!\n");
    }

    /* No thread change, don't do a context switch */
    if (c_thread == target_thread->t)
        return -1;

    ASSERT_MSG(target_thread->c_queue, "Tried to enqueue a thread with no queue");

    // KLOG_DBG("Scheduler: Switching contexts! (%s:%s -> %s:%s)\n", c_thread->m_parent_proc->m_name, c_thread->m_name, target_thread->t->m_parent_proc->m_name, target_thread->t->m_name);

    /* Set the previous and current threads */
    set_previous_thread(c_thread);
    set_current_handled_thread(target_thread->t);
    set_current_proc(target_thread->t->m_parent_proc);

    /* Do the conext switch */
    thread_switch_context(c_thread, target_thread->t);

    return 0;
}

void scheduler_yield()
{
    scheduler_t* s;
    processor_t* current;

    s = get_current_scheduler();

    ASSERT_MSG(s, "Tried to yield without a scheduler!");
    ASSERT_MSG((s->flags & SCHEDULER_FLAG_RUNNING) == SCHEDULER_FLAG_RUNNING, "Tried to yield to an unstarted scheduler!");
    ASSERT_MSG(!s->pause_depth, "Tried to yield to a paused scheduler!");

    CHECK_AND_DO_DISABLE_INTERRUPTS();

    scheduler_do_tick(s, NULL, true);

    current = get_current_processor();

    // in this case we don't have to wait for us to exit a
    // critical CPU section, since we are not being interrupted at all
    if (current->m_irq_depth == 0)
        scheduler_try_execute(current);

    CHECK_AND_TRY_ENABLE_INTERRUPTS();
}
/*!
 * @brief: Single scheduler tick
 *
 * Only called when the scheduler is actually running
 *
 * This function does the scaffolding for general sthread scheduling. It first
 * tries to tick the current active sthread. When there is no sthread to tick
 * left in the current active queue, it checks if the inactive queue has threads.
 * if it is not the case, the scheduler ends the epoch and switches to the idle thread
 * for this CPU. otherwise, the scheduler enters a new epoch and switches the active and
 * inactive queues.
 */
static inline void scheduler_do_tick(scheduler_t* sched, u64 tick_delta, bool force)
{
    thread_t* t;

    t = get_current_scheduling_thread();

    if (!t || !t->sthread)
        return;

    /* No need to tick if this thread isn't even in a queue */
    if (!t->sthread->c_queue)
        return;

    // ASSERT_MSG(t->sthread, "Ticking no thread...");

    /* Don't force the reschedule */
    try_do_sthread_tick(t->sthread, tick_delta, force);
}

/*!
 * @brief: The beating heart of the scheduler
 */
static void sched_tick(scheduler_t* s, registers_t* registers_ptr, u64 timeticks)
{
    /* Do a scheduler tick */
    scheduler_do_tick(s, (timeticks - s->last_nr_ticks) * STIMESLICE_STEPPING, false);

    /* Update the previous tick count */
    s->last_nr_ticks = timeticks;
}

/*!
 * @brief: Adds a single thread to the schedule
 */
kerror_t scheduler_add_thread(thread_t* thread, enum SCHEDULER_PRIORITY prio)
{
    return scheduler_add_thread_ex(get_current_scheduler(), thread, prio);
}

kerror_t scheduler_add_thread_ex(scheduler_t* s, thread_t* thread, enum SCHEDULER_PRIORITY prio)
{
    if (!s || !thread)
        return -KERR_INVAL;

    pause_scheduler();

    /* Grab the direct pointer */
    if (!thread->sthread) {
        /* New thread, create a new sthread for it */
        thread->sthread = create_sthread(s, thread, prio);

        ASSERT_MSG(thread->sthread, "Scheduler: Failed to create sthread!");

        /*
         * Only prepare the context here if we're not trying to add the init thread
         *
         * When adding a seperate thread to a process, we have time to alter the thread between creating it and
         * adding it. We don't have this time with the initial thread, which has it's context prepared right before it
         * is scheduled for the first time
         */
        thread_prepare_context(thread);
    }

    /*
     * Enqueue the bitch in the expired list. This way we don't mess up
     * the current epoch
     */
    squeue_enqueue(s->active_q, thread->sthread);

    /* Recalculate the timeslice, just in case */
    sthread_calc_stimeslice(thread->sthread);

    resume_scheduler();
    return 0;
}

kerror_t scheduler_remove_thread(thread_t* thread)
{
    return scheduler_remove_thread_ex(get_current_scheduler(), thread);
}

/*!
 * @brief: Remove a thread from the scheduler
 */
kerror_t scheduler_remove_thread_ex(scheduler_t* s, thread_t* thread)
{
    int error = -KERR_NOT_FOUND;

    pause_scheduler();

    /* This would mean this thread is not anywhere in this shceduler */
    if (!thread->sthread || !thread->sthread_slot)
        goto resume_and_error;

    /* Remove the thread from it's queue, if it's in one */
    if (thread->sthread->c_queue)
        squeue_remove(thread->sthread->c_queue, thread->sthread_slot);

    /* Destroy the sthread, we don't need it anymore */
    destroy_sthread(s, thread->sthread);

    /* Removed, xD */
    thread->sthread_slot = NULL;
    thread->sthread = NULL;

    error = 0;
resume_and_error:
    resume_scheduler();
    return error;
}

kerror_t scheduler_inactivate_thread(thread_t* thread)
{
    return scheduler_inactivate_thread_ex(get_current_scheduler(), thread);
}

/*!
 * @brief: Remove a thread from the scheduler queue
 *
 * Prevent a thread from being scheduled, by removing the thread from the scheduler queue
 */
kerror_t scheduler_inactivate_thread_ex(scheduler_t* s, thread_t* thread)
{
    sthread_t* sthread;

    if (!s || !thread)
        return -KERR_INVAL;

    if (!thread->sthread_slot)
        return -KERR_INVAL;

    pause_scheduler();

    sthread = thread->sthread;

    if (!sthread || !thread->sthread_slot) {
        resume_scheduler();
        return -KERR_INVAL;
    }

    /*
     * Remove the scheduler thread from it's queue
     *
     * This will remove the thread from the schedule and link the thread
     * to itself. This indicates an inactive thread
     */
    squeue_remove(sthread->c_queue, thread->sthread_slot);

    resume_scheduler();
    return 0;
}

/*!
 * @brief: Add all the threads of a process to the current scheduler
 */
kerror_t scheduler_add_proc(proc_t* p, enum SCHEDULER_PRIORITY prio)
{
    return scheduler_add_proc_ex(get_current_scheduler(), p, prio);
}

/*!
 * @brief: Adds all the threads of a single process to the scheduler
 *
 * NOTE: All the threads currently in the process will all recieve the same base priority
 */
kerror_t scheduler_add_proc_ex(scheduler_t* scheduler, proc_t* p, enum SCHEDULER_PRIORITY prio)
{
    int error = 0;
    thread_t* c_thread;

    if (!scheduler || !p)
        return -KERR_INVAL;

    KLOG_DBG("Scheduler: Adding proc %s\n", p->m_name);

    FOREACH(i, p->m_threads)
    {
        c_thread = i->data;

        error = scheduler_add_thread_ex(scheduler, c_thread, prio);

        if (error)
            break;
    }

    return error;
}

/*!
 * @brief: Remove all threads of a process from the current scheduler
 */
kerror_t scheduler_remove_proc(proc_t* p)
{
    return scheduler_remove_proc_ex(get_current_scheduler(), p);
}

/*!
 * @brief: Remove all threads from a process from the scheduelr
 *
 * Loops over all the threads inside a process and remove each one of them.
 */
kerror_t scheduler_remove_proc_ex(scheduler_t* scheduler, proc_t* p)
{
    int error = 0;
    thread_t* c_thread;

    if (!scheduler || !p)
        return -KERR_INVAL;

    /* Pause the scheduler just in case */
    pause_scheduler();

    FOREACH(i, p->m_threads)
    {
        c_thread = i->data;

        error = scheduler_remove_thread_ex(scheduler, c_thread);

        if (error)
            break;
    }

    /* Make sure the scheduler is resumed */
    resume_scheduler();

    return error;
}

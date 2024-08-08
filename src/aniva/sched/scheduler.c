#include "scheduler.h"
#include "entry/entry.h"
#include "irq/interrupts.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/zalloc/zalloc.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "system/asm_specifics.h"
#include "system/processor/processor.h"
#include <mem/heap.h>

// --- inline functions ---
static ALWAYS_INLINE thread_t* pull_runnable_thread(scheduler_t* s);
static ALWAYS_INLINE void set_previous_thread(thread_t* thread);
static ALWAYS_INLINE void set_current_proc(proc_t* proc);

/* Default scheduler tick function */
static registers_t* sched_tick(registers_t* registers_ptr);

/*!
 * @brief: Helper to easily track interrupt disables and disable
 */
static inline void scheduler_try_disable_interrupts(scheduler_t* s)
{
    if (!interrupts_are_enabled() && !s->interrupt_depth)
        s->interrupt_depth++;

    disable_interrupts();

    if (!s)
        return;

    s->interrupt_depth++;
}

/*!
 * @brief: Helper to easily track interrupt enables and enable
 */
static inline void scheduler_try_enable_interrupts(scheduler_t* s)
{
    if (!s)
        return;

    if (s->interrupt_depth)
        s->interrupt_depth--;

    if (!s->interrupt_depth)
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

    this->lock = create_mutex(0);
    this->sthread_allocator = create_zone_allocator(64 * Kib, sizeof(sthread_t), NULL);
    this->f_tick = sched_tick;

    init_squeue(&this->queues[0]);
    init_squeue(&this->queues[1]);

    /* Initialize the queues */
    this->active_q = &this->queues[0];
    this->inactive_q = &this->queues[1];

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
    proc_t* initial_process;
    thread_t* initial_thread;
    sched_frame_t* frame_ptr;

    sched = get_current_scheduler();

    if (!sched)
        return;

    frame_ptr = scheduler_queue_peek(&sched->processes);

    if (!frame_ptr)
        return;

    initial_process = frame_ptr->m_proc;

    ASSERT_MSG((initial_process->m_flags & PROC_KERNEL) == PROC_KERNEL, "FATAL: initial process was not a kernel process");

    // let's jump into the idle thread initially, so we can then
    // resume to the thread-pool when we leave critical sections
    initial_thread = initial_process->m_init_thread;

    /* Set some processor state */
    set_current_proc(frame_ptr->m_proc);
    set_current_handled_thread(initial_thread);
    set_previous_thread(nullptr);

    sched->flags |= SCHEDULER_FLAG_RUNNING;
    g_system_info.sys_flags |= SYSFLAGS_HAS_MULTITHREADING;

    bootstrap_thread_entries(initial_thread);
}

/*!
 * Try to fetch another thread for us to schedule.
 * If this current process is in idle mode, we just
 * schedule the idle thread
 * @force: Tells the routine wether or not to force a reschedule. This is set when the scheduler
 * yields, which means the calling thread wants to drain it's tick count and move execution to the next
 * available thread
 *
 * @returns: true if we should reschedule, false otherwise
 */
bool try_do_schedule(scheduler_t* sched, sched_frame_t* frame, bool force)
{
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

    scheduler_try_disable_interrupts(s);

    /* If we are trying to pause inside of a pause, just increment the pause depth */
    if (s->pause_depth)
        goto skip_mutex;

    mutex_lock(s->lock);

    s->flags |= SCHEDULER_FLAG_PAUSED;

skip_mutex:
    s->pause_depth++;

    scheduler_try_enable_interrupts(s);
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

    scheduler_try_disable_interrupts(s);

    /* Only decrement if we can */
    if (s->pause_depth)
        s->pause_depth--;

    /* Only unlock if we just decremented to the last pause level =) */
    if (!s->pause_depth) {
        // make sure the scheduler is in the right state
        s->flags &= ~SCHEDULER_FLAG_PAUSED;

        /* Unlock the mutex */
        mutex_unlock(s->lock);
    }

    scheduler_try_enable_interrupts(s);
    return ANIVA_SUCCESS;
}

static inline bool scheduler_has_request(scheduler_t* s)
{
    return (s->flags & SCHEDULER_FLAG_HAS_REQUEST) == SCHEDULER_FLAG_HAS_REQUEST;
}

static inline void scheduler_clear_request(scheduler_t* s)
{
    s->flags &= ~SCHEDULER_FLAG_HAS_REQUEST;
}

/*!
 * @brief: Notify the scheduler that we want to reschedule
 */
void scheduler_set_request(scheduler_t* s)
{
    s->flags |= SCHEDULER_FLAG_HAS_REQUEST;
}

void scheduler_yield()
{
    scheduler_t* s;
    sched_frame_t* frame;
    processor_t* current;
    thread_t* current_thread;

    s = get_current_scheduler();

    ASSERT_MSG(s, "Tried to yield without a scheduler!");
    ASSERT_MSG((s->flags & SCHEDULER_FLAG_RUNNING) == SCHEDULER_FLAG_RUNNING, "Tried to yield to an unstarted scheduler!");
    ASSERT_MSG(!s->pause_depth, "Tried to yield to a paused scheduler!");

    CHECK_AND_DO_DISABLE_INTERRUPTS();

    frame = scheduler_queue_peek(&s->processes);

    ASSERT_MSG(frame, "Could not yield on an NULL schedframe");

    // prepare the next thread
    if (try_do_schedule(s, frame, true))
        scheduler_set_request(s);

    current = get_current_processor();
    current_thread = get_current_scheduling_thread();

    ASSERT_MSG(current_thread != nullptr, "trying to yield the scheduler while not having a current scheduling thread!");

    // in this case we don't have to wait for us to exit a
    // critical CPU section, since we are not being interrupted at all
    if (current->m_irq_depth == 0 && atomic_ptr_read(current->m_critical_depth) == 0)
        scheduler_try_execute();

    CHECK_AND_TRY_ENABLE_INTERRUPTS();
}

/*!
 * @brief: Try to execute a schedule on the current scheduler
 *
 * NOTE: this requires the scheduler to be paused
 */
int scheduler_try_execute()
{
    processor_t* p;
    scheduler_t* s;

    p = get_current_processor();
    s = get_current_scheduler();

    ASSERT_MSG(p && s, "Could not get current processor while trying to calling scheduler");
    ASSERT_MSG(p->m_irq_depth == 0, "Trying to call scheduler while in irq");

    if (atomic_ptr_read(p->m_critical_depth))
        return -1;

    /* If we don't want to schedule, or our scheduler is paused, return */
    if (!scheduler_has_request(s))
        return -1;

    scheduler_clear_request(s);

    thread_t* next_thread = get_current_scheduling_thread();
    thread_t* prev_thread = get_previous_scheduled_thread();

    thread_switch_context(prev_thread, next_thread);

    return 0;
}

static registers_t* schedframe_tick(scheduler_t* sched, sched_frame_t* frame, registers_t* regs)
{
    /* Don't force the reschedule */
    if (try_do_schedule(sched, frame, false))
        scheduler_set_request(sched);

    return regs;
}

/*!
 * @brief: The beating heart of the scheduler
 */
static registers_t* sched_tick(registers_t* registers_ptr)
{
    scheduler_t* this;
    sched_frame_t* current_frame;

    this = get_current_scheduler();

    if (!this)
        return registers_ptr;

    if ((this->flags & SCHEDULER_FLAG_RUNNING) != SCHEDULER_FLAG_RUNNING)
        return registers_ptr;

    if (this->pause_depth || mutex_is_locked(this->lock))
        return registers_ptr;

    current_frame = scheduler_queue_peek(&this->processes);

    ASSERT_MSG(current_frame, "Failed to get the current scheduler frame!");

    return schedframe_tick(this, current_frame, registers_ptr);
}

/*
 * TODO: redo this function, as it is way too messy and
 * should be as optimal as possible
 * perhaps we can use a queue or something (it's unclear to me which ds works best here...)
 *
 * Returning nullptr means that this thread does not have any threads left. Let's just kill it at that point...
 */
static ALWAYS_INLINE thread_t* pull_runnable_thread(scheduler_t* s)
{
}

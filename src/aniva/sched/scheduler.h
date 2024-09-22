#ifndef __ANIVA_SCHEDULER__
#define __ANIVA_SCHEDULER__

#include "libk/flow/error.h"
#include "logging/log.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/time.h"
#include "time/core.h"
#include <mem/zalloc/zalloc.h>

struct squeue;
struct sthread;
struct scheduler;

/*
 * LightOS Scheduler v2
 *
 * I was unhappy with the clunky architecture of the previous scheduler and after reading a paper on
 * the linux 2.6.x kernels O(1) scheduler, I felt inclined to fix my own implementation.
 * NOTE: It is important that our scheduler code is as efficient as possible, since it's going to get ran very
 * often.
 *
 * The new implementation:
 *
 * We're finally going to do some actual scheduling. We're going to be talking in terms of epochs, meaning
 * the timespan in which all running threads on a given scheduler have used up their timeslices. Thus,
 * threads get a 'slice' of this epoch time.
 * We're also going to try to make a destinction between CPU-bound threads and I/O-bound threads, like linux.
 * Overall, I/O-bound threads need priority inside the scheduler, since they need their time to wait for I/O
 * opperations to finish, which makes it easier to notice for the user when there are performance issues in
 * these threads. Threads can mark themselves as CPU- or I/O-bound, or we can try to make a guess, based
 * on the syscalls or on the kernel opperations a thread is doing
 */

enum SCHEDULER_PRIORITY {
    SCHED_PRIO_LOWEST, // Thread is of the lowest conrern to us
    SCHED_PRIO_LOW, // We can't really give 2 fucks about this thread
    SCHED_PRIO_NONESSENTIAL, // This thread is doing 'non-essential' work
    SCHED_PRIO_MID, // This thread is just a regular boi
    SCHED_PRIO_HIGH, // This is a special thread
    SCHED_PRIO_IMPORTANT, // This thread is doing important work
    SCHED_PRIO_SUPER_IMPORTANT, // This thread really needs it's CPU time
    SCHED_PRIO_HIGHEST, // The most special thread

    /*
     * Prios in terms of simple numbers, where a lower
     * number means a lower scheduler prio
     */
    SCHED_PRIO_0 = 0,
    SCHED_PRIO_1,
    SCHED_PRIO_2,
    SCHED_PRIO_3,
    SCHED_PRIO_4,
    SCHED_PRIO_5,
    SCHED_PRIO_6,
    SCHED_PRIO_7,

    /* Max static priority a process may have */
    SCHED_PRIO_MAX = SCHED_PRIO_7,

    N_SCHED_PRIO
};

#define SCHEDULER_FLAG_FORCE_REQUEUE 0x00000001
#define SCHEDULER_FLAG_PAUSED 0x00000002
/* Do we need to reorder the process list? */
#define SCHEDULER_FLAG_NEED_REORDER 0x00000004
#define SCHEDULER_FLAG_RUNNING 0x00000008

#define SCHEDULER_PRIORITY_MAX 0x8000
#define SCHEDULER_PRIORITY_DYDX 0x1000
#define SCHEDULER_PRIORITY_MAX_LOG2 15

/*
 * Calculate the actual priority for a single sthread
 * This assumes SCHED_PRIO_CHANGED_BIT is cleared
 *
 * base_prio (n) : 0 -> 7
 *
 * P(n) = an - b
 * a = SCHEDULER_PRIORITY_DYDX
 * b = penalty
 */
#define SCHEDULER_CALC_PRIO(st) ((SCHEDULER_PRIORITY_DYDX * (st->base_prio + 1)) - ((u16)st->prio_penalty));

/*
 * Scheduler thread
 *
 * Wrapper around a single thread, as seen from the schedulers perspective
 * This struct contains some data which the scheduler needs to do it's scheduling
 */
typedef struct sthread {
    /* The thread we're dealing with */
    thread_t* t;

    /* The current slice this thread has got since the last epoch */
    stimeslice_t tslice;
    /* How much of the slice has elapsed */
    stimeslice_t elapsed_tslice;

    /* Static thread priority of this thread */
    enum SCHEDULER_PRIORITY base_prio;
    /*
     * The actual priority level this thread has.
     * Recalculated every time the scheduler switches away
     * from this thread
     */
    u16 actual_prio;
    /*
     * Priority scores given to this thread by the scheduler
     * Can't be higher than SCHEDULER_PRIORITY_DYDX
     */
    u8 prio_penalty;
    u8 res;

    /* The queue we're currently in */
    struct squeue* c_queue;

    /* sthreads with the same scheduler priority are put into a linked list */
    struct sthread* next;
} sched_thread_t, sthread_t;

/* sthread.c */
extern sthread_t* create_sthread(struct scheduler* s, thread_t* t, enum SCHEDULER_PRIORITY p);
extern void destroy_sthread(struct scheduler* s, sthread_t* st);

static inline void sthread_calc_stimeslice(sthread_t* st)
{
    st->actual_prio = SCHEDULER_CALC_PRIO(st);

    /* Calculate a new timeslice */
    st->tslice = STIMESLICE(st);
    st->elapsed_tslice = 0;
}

/*!
 * @brief: Ticks a single sthread
 *
 * Removes the appropriate amount of time from the current sthreads time quota.
 * If there seems to be no active thread in the current active queue, this function
 * tries to find a new thread in one of the lower priority lists.
 *
 * @returns: true if the scheduler needs to do a sthread reschedule, false if there
 * is nothing to be done.
 */
static inline void try_do_sthread_tick(sthread_t* t, stimeslice_t delta, bool force)
{
    /* Force us to add an entire granularity of timeslice, which forces context switch */
    if (force)
        delta = STIMESLICE_GRANULARITY;

    /* Add a single stepping to the timeslice */
    t->elapsed_tslice += delta;
}

static inline bool sthread_needs_requeue(sthread_t* s)
{
    return (!s || !s->c_queue || s->tslice <= 0 || s->elapsed_tslice >= s->tslice || s->elapsed_tslice >= STIMESLICE_GRANULARITY);
}

typedef struct sthread_list_head {
    struct sthread* list;
    struct sthread** enq;
} sthread_list_head_t;

/*
 * A scheduler thread queue
 */
typedef struct squeue {
    /* An array of sthread lists */
    sthread_list_head_t vec_threads[N_SCHED_PRIO];

    /* The current active priority that's being scheduled */
    u32 active_prio;
    /* The sthread count in this queue */
    u32 n_sthread;
} scheduler_queue_t, squeue_t;

/* queue.c */
extern kerror_t init_squeue(scheduler_queue_t* out);
extern kerror_t squeue_clear(scheduler_queue_t* queue);

static inline sthread_t* squeue_get_active_threadlist(squeue_t* q)
{
    return q->vec_threads[q->active_prio].list;
}

/*!
 * @brief: Do a single scheduler priority shift
 *
 * The scheduler runs from high priority down to lower priority, with wraparound
 */
static inline sthread_t* squeue_next_prio(squeue_t* q)
{
    sthread_t* ret;

    for (u32 i = 0; i < (N_SCHED_PRIO + 1); i++) {
        if (q->active_prio)
            q->active_prio--;
        else
            q->active_prio = SCHED_PRIO_MAX;

        ret = squeue_get_active_threadlist(q);

        if (ret)
            return ret;
    }

    KLOG_DBG("q_thing=%d\n", q->n_sthread);
    kernel_panic("what");
    return nullptr;
}

static inline sthread_t* squeue_next_priority_thread(squeue_t* q)
{
    ASSERT(q->n_sthread);

    return squeue_next_prio(q);
}

void squeue_remove(scheduler_queue_t* queue, struct sthread** t);
void squeue_enqueue(scheduler_queue_t* queue, struct sthread* t);

/*
 * This is the structure that holds information about a processors
 * scheduler
 */
typedef struct scheduler {
    /* Lock to make sure only one thread accesses the scheduler at once */
    spinlock_t* lock;
    /* How many layered pause calls have we already had */
    uint32_t pause_depth;
    /* How often did we try to disable interrupts */
    uint32_t int_disable_depth;
    /* Scheduler status flags */
    uint32_t flags;
    /* How many timer ticks left until the scheduler gets called again */
    uint32_t active_q_idx;

    /* How many systicks had elapsed last scheduler tick */
    u64 last_nr_ticks;

    /*
     * We have two queues to move threads between.
     * The inactive queue is also where we put threads
     * that are inserted during an epoch. These threads
     * will get their timeslices calculated at the start
     * of the next epoch
     */
    squeue_t queues[2];

    /* The idle thread for this CPU/scheduler */
    sthread_t* idle;

    /* sthread cache */
    zone_allocator_t* sthread_allocator;

    /* Base time at the time of creating the scheduler */
    system_time_t scheduler_base_time;
} scheduler_t;

static inline squeue_t* scheduler_get_active_q(scheduler_t* s)
{
    return &s->queues[s->active_q_idx];
}

static inline sthread_t* scheduler_get_new_thread(scheduler_t* s, sthread_t* c_active)
{
    squeue_t* active = scheduler_get_active_q(s);

    /* Check if this priority has threads left */
    if (!c_active || !c_active->c_queue || !squeue_get_active_threadlist(active))
        return squeue_next_priority_thread(active);

    /* Grab the next fucker here */
    if (c_active->next)
        return c_active->next;

    /* Start again at the start of this list */
    return squeue_get_active_threadlist(active);
}

static inline bool scheduler_is_paused(scheduler_t* s)
{
    return (s->pause_depth > 0);
}

/*
 * Switch the queues by changing the active idx
 */
static inline void scheduler_switch_queues(scheduler_t* s)
{
    s->active_q_idx = (s->active_q_idx + 1) & 1;
}

/*
 * initialization
 */
ANIVA_STATUS init_scheduler(uint32_t cpu);

/* control */
void start_scheduler(void);

ANIVA_STATUS resume_scheduler(void);
ANIVA_STATUS pause_scheduler();

/*
 * yield to the scheduler and let it switch to a new thread
 */
void scheduler_yield();
int scheduler_try_execute(struct processor* p, bool force_rq);
void scheduler_tick(scheduler_t* s, u64 elapsed_ticks);

kerror_t scheduler_add_thread(thread_t* thread, enum SCHEDULER_PRIORITY prio);
kerror_t scheduler_add_thread_ex(scheduler_t* s, thread_t* thread, enum SCHEDULER_PRIORITY prio);
kerror_t scheduler_remove_thread(thread_t* thread);
kerror_t scheduler_remove_thread_ex(scheduler_t* s, thread_t* thread);
kerror_t scheduler_inactivate_thread(thread_t* thread);
kerror_t scheduler_inactivate_thread_ex(scheduler_t* s, thread_t* thread);
/*
 * Try and insert a new process into the scheduler
 * to be scheduled on the next reschedule. This function
 * either agressively inserts itself at the front of the
 * process selection or simply puts itself behind the current
 * running process to be scheduled next, based on the reschedule param
 */
kerror_t scheduler_add_proc(proc_t* p, enum SCHEDULER_PRIORITY prio);
kerror_t scheduler_add_proc_ex(scheduler_t* scheduler, proc_t* p, enum SCHEDULER_PRIORITY prio);
kerror_t scheduler_remove_proc(proc_t* p);
kerror_t scheduler_remove_proc_ex(scheduler_t* scheduler, proc_t* p);

thread_t* get_current_scheduling_thread();
thread_t* get_previous_scheduled_thread();
proc_t* get_current_proc();
proc_t* sched_get_kernel_proc();

kerror_t set_kernel_proc(proc_t* proc);

scheduler_t* get_current_scheduler();
scheduler_t* get_scheduler_for(uint32_t cpu);

#endif // !__ANIVA_SCHEDULER__

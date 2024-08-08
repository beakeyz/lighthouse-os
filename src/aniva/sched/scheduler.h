#ifndef __ANIVA_SCHEDULER__
#define __ANIVA_SCHEDULER__
#include "libk/flow/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/time.h"
#include "system/processor/registers.h"
#include <mem/zalloc/zalloc.h>

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
    /* The number of scheduler priority levels */
    N_SCHED_PRIO,

    /*
     * This bit will be set when there was a priority change
     * We'll check for this bit every time we switch away from
     * an sthread and if it's set, we'll clear it and recalculate
     * the thread priority
     */
    SCHED_PRIO_CHANGED_BIT = 0x80000000,
};

#define SCHEDULER_FLAG_HAS_REQUEST 0x00000001
#define SCHEDULER_FLAG_PAUSED 0x00000002
/* Do we need to reorder the process list? */
#define SCHEDULER_FLAG_NEED_REORDER 0x00000004
#define SCHEDULER_FLAG_RUNNING 0x00000008

#define SCHEDULER_PRIORITY_MAX 0x7fff
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
#define SCHEDULER_CALC_PRIO(st) ((SCHEDULER_PRIORITY_DYDX * (st->base_prio + 1)) - ((u16)st->prio_penalty * 4));

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
    /* Priority scores given to this thread by the scheduler */
    u16 prio_penalty;

    /* sthreads with the same scheduler priority are put into a linked list */
    struct sthread* next;
} sched_thread_t, sthread_t;

/*
 * A scheduler thread queue
 */
typedef struct squeue {
    /* An array of sthread lists */
    struct sthread* vec_threads[N_SCHED_PRIO];

    /* The sthread count in this queue */
    u32 n_sthread;
} scheduler_queue_t, squeue_t;

/* queue.c */
extern kerror_t init_squeue(scheduler_queue_t* out);
extern kerror_t squeue_clear(scheduler_queue_t* queue);
extern kerror_t squeue_enqueue(scheduler_queue_t* queue, struct thread* t);
extern struct sched_frame* squeue_dequeue(scheduler_queue_t* queue);
extern struct sched_frame* squeue_peek(scheduler_queue_t* queue);

/*
 * This is the structure that holds information about a processors
 * scheduler
 */
typedef struct scheduler {
    /* Lock to make sure only one thread accesses the scheduler at once */
    mutex_t* lock;
    uint32_t pause_depth;
    uint32_t interrupt_depth;
    uint32_t flags;

    /*
     * We have two queues to move threads between.
     * The inactive queue is also where we put threads
     * that are inserted during an epoch. These threads
     * will get their timeslices calculated at the start
     * of the next epoch
     */
    squeue_t queues[2];
    /* Queue where active threads sit (threads which still have an active timeslice) */
    squeue_t* active_q;
    /* Queue where all the threads go that have used up their timeslice of this epoch */
    squeue_t* inactive_q;

    /* The current running thread on the scheduler */
    sthread_t* c_thread;
    /* The idle thread for this CPU/scheduler */
    sthread_t* idle;

    /* sthread cache */
    zone_allocator_t* sthread_allocator;

    /* Ticker function. Called every scheduler interrupt */
    registers_t* (*f_tick)(registers_t* regs);
} scheduler_t;

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

kerror_t scheduler_add_thread(thread_t* thread, enum SCHEDULER_PRIORITY prio);
kerror_t scheduler_remove_thread(thread_t* thread);
/*
 * Try and insert a new process into the scheduler
 * to be scheduled on the next reschedule. This function
 * either agressively inserts itself at the front of the
 * process selection or simply puts itself behind the current
 * running process to be scheduled next, based on the reschedule param
 */
kerror_t scheduler_add_proc(proc_t* p);
kerror_t scheduler_remove_proc(proc_t* p);

thread_t* get_current_scheduling_thread();
thread_t* get_previous_scheduled_thread();
proc_t* get_current_proc();
proc_t* sched_get_kernel_proc();

kerror_t set_kernel_proc(proc_t* proc);

scheduler_t* get_current_scheduler();
scheduler_t* get_scheduler_for(uint32_t cpu);

#endif // !__ANIVA_SCHEDULER__

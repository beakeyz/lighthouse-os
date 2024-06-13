#ifndef __ANIVA_SCHEDULER__
#define __ANIVA_SCHEDULER__
#include "libk/flow/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/queue.h"
#include "system/processor/registers.h"

/*
 * The current model is a simple fifo algorithm, wich does not take into
 * account how much time a process needs to do its thing or how much resources
 * it is taking up (same goes for threads). We want to be able to optimize this
 * easily in the future, so let's make sure we can do that
 * (TODO)
 */

#define SCHED_FRAME_FLAG_RUNNABLE 0x00000001
#define SCHED_FRAME_FLAG_RUNNING 0x00000002
#define SCHED_FRAME_FLAG_BLOCKED 0x00000004
/* TODO: this useful? */
#define SCHED_FRAME_FLAG_MUST_RESCEDULE 0x00000008

enum SCHEDULER_PRIORITY {
    SCHED_PRIO_LOW = 1,
    SCHED_PRIO_MID = 2,
    SCHED_PRIO_HIGH = 4,
    SCHED_PRIO_HIGHEST = 8,
};

typedef struct sched_frame {
    proc_t* m_proc;
    size_t m_frame_ticks;
    size_t m_max_ticks;
    uint32_t m_scheduled_thread_index;
    uint32_t m_flags;

    /* Item after us in the queue */
    struct sched_frame* previous;
} sched_frame_t;

#define SCHEDULER_FLAG_HAS_REQUEST 0x00000001
#define SCHEDULER_FLAG_PAUSED 0x00000002
/* Do we need to reorder the process list? */
#define SCHEDULER_FLAG_NEED_REORDER 0x00000004
#define SCHEDULER_FLAG_RUNNING 0x00000008

/*
 * This is the structure that holds information about a processors
 * scheduler
 */
typedef struct scheduler {
    // list_t* scheduler_frames;
    scheduler_queue_t processes;
    mutex_t* lock;
    uint32_t pause_depth;
    uint32_t interrupt_depth;
    uint32_t flags;

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
 * pick the next thread to run in the current sched frame
 */
bool try_do_schedule(scheduler_t* sched, sched_frame_t* frame, bool force);

/*
 * yield to the scheduler and let it switch to a new thread
 */
void scheduler_yield();

/* die */
int scheduler_try_execute();
void scheduler_set_request(scheduler_t* s);

/*
 * Try and insert a new process into the scheduler
 * to be scheduled on the next reschedule. This function
 * either agressively inserts itself at the front of the
 * process selection or simply puts itself behind the current
 * running process to be scheduled next, based on the reschedule param
 */
kerror_t sched_add_priority_proc(proc_t*, enum SCHEDULER_PRIORITY prio, bool reschedule);
ANIVA_STATUS sched_add_proc(proc_t*, enum SCHEDULER_PRIORITY prio);

ANIVA_STATUS sched_remove_proc(proc_t*);
ANIVA_STATUS sched_remove_thread(thread_t*);

thread_t* get_current_scheduling_thread();
thread_t* get_previous_scheduled_thread();
proc_t* get_current_proc();

scheduler_t* get_current_scheduler();
scheduler_t* get_scheduler_for(uint32_t cpu);

void set_current_handled_thread(thread_t* thread);

proc_t* sched_get_kernel_proc();
void set_kernel_proc(proc_t* proc);

#endif // !__ANIVA_SCHEDULER__

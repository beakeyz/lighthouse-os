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

#define SCHED_FRAME_DEFAULT_START_TICKS 2

#define SCHED_FRAME_FLAG_RUNNABLE 0x00000001
#define SCHED_FRAME_FLAG_RUNNING 0x00000002
#define SCHED_FRAME_FLAG_BLOCKED 0x00000004
/* TODO: this useful? */
#define SCHED_FRAME_FLAG_MUST_RESCEDULE 0x00000008

// how long this process takes to do it's shit
enum SCHED_FRAME_USAGE_LEVEL {
  SUPER_LOW = 0,
  LOW,
  MID,
  HIGH,
  SEVERE,
  MIGHT_BE_DEAD
};

typedef struct sched_frame {
  proc_t* m_proc_to_schedule;
  size_t m_sched_ticks_left;
  size_t m_max_ticks;
  uint32_t m_scheduled_thread_index;
  uint32_t m_flags;

  enum SCHED_FRAME_USAGE_LEVEL m_fram_usage_lvl;

  /* Item after us in the queue */
  struct sched_frame* previous;
} sched_frame_t;

#define SCHEDULER_FLAG_HAS_REQUEST      0x00000001
#define SCHEDULER_FLAG_PAUSED           0x00000002
/* Do we need to reorder the process list? */
#define SCHEDULER_FLAG_NEED_REORDER     0x00000004
#define SCHEDULER_FLAG_RUNNING          0x00000008
/* Did we have interrupts enabled before we paused the scheduler? */
#define SCHEDULER_FLAG_HAD_INTERRUPTS   0x00000010

/*
 * This is the structure that holds information about a processors 
 * scheduler
 */
typedef struct scheduler {
  //list_t* scheduler_frames;
  scheduler_queue_t processes;
  mutex_t* lock;
  uint32_t pause_depth;
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
ErrorOrPtr pick_next_thread_scheduler(void);

/*
 * yield to the scheduler and let it switch to a new thread
 */
void scheduler_yield();

/* die */
ErrorOrPtr scheduler_try_execute();
void scheduler_set_request(scheduler_t* s);


/*
 * Try and insert a new process into the scheduler
 * to be scheduled on the next reschedule. This function
 * either agressively inserts itself at the front of the 
 * process selection or simply puts itself behind the current 
 * running process to be scheduled next, based on the reschedule param
 */
ErrorOrPtr sched_add_priority_proc(proc_t*, bool reschedule);
ANIVA_STATUS sched_add_proc(proc_t*);

ANIVA_STATUS sched_remove_proc(proc_t*);
ANIVA_STATUS sched_remove_proc_by_id(proc_id_t);
ANIVA_STATUS sched_remove_thread(thread_t*);

thread_t *get_current_scheduling_thread();
thread_t *get_previous_scheduled_thread();
scheduler_t* get_current_scheduler();
proc_t* get_current_proc();

void set_current_handled_thread(thread_t* thread);

proc_t* sched_get_kernel_proc();
void set_kernel_proc(proc_t* proc);

#endif // !__ANIVA_SCHEDULER__

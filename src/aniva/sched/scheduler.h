#ifndef __ANIVA_SCHEDULER__
#define __ANIVA_SCHEDULER__
#include "libk/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "system/processor/registers.h"

#define SCHED_FRAME_DEFAULT_START_TICKS 2

/*
 * initialization
 */
ANIVA_STATUS init_scheduler();

/* control */
void start_scheduler(void);
void resume_scheduler(void);
ANIVA_STATUS pause_scheduler();

bool sched_can_schedule();

/*
 * pick the next thread to run in the current sched frame
 */
void pick_next_thread_scheduler(void);

/*
 * yield to the scheduler and let it switch to a new thread
 */
void scheduler_yield();

/* die */
ErrorOrPtr scheduler_try_execute();
ErrorOrPtr scheduler_try_invoke();

registers_t *sched_tick(registers_t*);

ANIVA_STATUS sched_add_proc(proc_t*);

ANIVA_STATUS sched_remove_proc(proc_t*);
ANIVA_STATUS sched_remove_proc_by_id(proc_id);
ANIVA_STATUS sched_remove_thread(thread_t*);

thread_t *get_current_scheduling_thread();
thread_t *get_previous_scheduled_thread();
void set_current_handled_thread(thread_t* thread);

proc_t* sched_get_kernel_proc();
void set_kernel_proc(proc_t* proc);

#endif // !__ANIVA_SCHEDULER__

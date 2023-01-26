#ifndef __ANIVA_SCHEDULER__
#define __ANIVA_SCHEDULER__
#include "libk/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "system/processor/registers.h"

#define SCHED_FRAME_DEFAULT_START_TICKS 100

/* initialization */
ANIVA_STATUS init_scheduler();
void scheduler_enter_first_thread(thread_t*);

/* control */
void start_scheduler(void);
void resume_scheduler(void);
ANIVA_STATUS pause_scheduler();
void pick_next_thread_scheduler(void);

/* die */
void scheduler_cleanup();

registers_t *sched_tick(registers_t*);

ANIVA_STATUS sched_add_proc(proc_t*);

ANIVA_STATUS sched_remove_proc(proc_t*);
ANIVA_STATUS sched_remove_proc_by_id(proc_id);
ANIVA_STATUS sched_remove_thread(thread_t*);

thread_t *get_current_scheduling_thread();
void set_current_handled_thread(thread_t* thread);
#endif // !__ANIVA_SCHEDULER__

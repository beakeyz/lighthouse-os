#ifndef __ANIVA_SCHEDULER__
#define __ANIVA_SCHEDULER__
#include "libk/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "system/processor/registers.h"

#define SCHED_DEFAULT_PROC_START_TICKS 10

/* initialization */
ANIVA_STATUS init_scheduler();

/* control */
void start_scheduler(void);
void resume_scheduler(void);
ANIVA_STATUS pause_scheduler();
ANIVA_STATUS sched_switch_context_to(thread_t*);
void pick_next_thread_scheduler(void);

/* die */
void scheduler_cleanup();

registers_t *sched_tick(registers_t*);

/* choose wich thread to switch to */
thread_t* sched_next_thread();

/* choose which process to give attention */
proc_t* sched_next_proc();

/* pull a funnie */
void sched_next();
void sched_safe_next();
bool sched_has_next();
void sched_first_switch_finished();


ANIVA_STATUS sched_add_proc(proc_t*, proc_id);
ANIVA_STATUS sched_add_thead(thread_t*);

ANIVA_STATUS sched_remove_proc(proc_t*);
ANIVA_STATUS sched_remove_proc_by_id(proc_id);
ANIVA_STATUS sched_remove_thread(thread_t*);


#endif // !__ANIVA_SCHEDULER__

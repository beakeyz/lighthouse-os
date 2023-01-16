#ifndef __LIGHT_SCHEDULER__
#define __LIGHT_SCHEDULER__
#include "libk/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "system/processor/registers.h"

/* initialization */
LIGHT_STATUS init_scheduler();

/* control */
void enter_scheduler();
LIGHT_STATUS exit_scheduler();
LIGHT_STATUS sched_switch_context_to(thread_t*);

/* die */
void scheduler_cleanup();

void sched_tick(registers_t*);

/* choose wich thread to switch to */
thread_t* sched_next_thread();

/* choose which process to give attention */
proc_t* sched_next_proc();

/* pull a funnie */
void sched_next();
void sched_safe_next();
bool sched_has_next();
void sched_first_switch_finished();

LIGHT_STATUS sched_add_proc(proc_t*, proc_id);
LIGHT_STATUS sched_add_thead(thread_t*);

LIGHT_STATUS sched_remove_proc(proc_t*);
LIGHT_STATUS sched_remove_proc_by_id(proc_id);
LIGHT_STATUS sched_remove_thread(thread_t*);


#endif // !__LIGHT_SCHEDULER__

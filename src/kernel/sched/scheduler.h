#ifndef __LIGHT_SCHEDULER__
#define __LIGHT_SCHEDULER__
#include "libk/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "system/processor/registers.h"

LIGHT_STATUS init_scheduler();

void enter_scheduler();
LIGHT_STATUS exit_scheduler();

LIGHT_STATUS switch_context_to(thread_t*);

void sched_tick(registers_t*);

thread_t* next_thread();
proc_t* next_proc();

LIGHT_STATUS sched_add_proc(proc_t*, proc_id);
LIGHT_STATUS sched_add_thead(thread_t*);

LIGHT_STATUS sched_remove_proc(proc_t*);
LIGHT_STATUS sched_remove_proc_by_id(proc_id);
LIGHT_STATUS sched_remove_thread(thread_t*);

#endif // !__LIGHT_SCHEDULER__

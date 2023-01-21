#ifndef __ANIVA_EVENT_CORE__
#define __ANIVA_EVENT_CORE__

#include "libk/error.h"
#include <libk/stddef.h>

struct thread;

struct time_update_event_hook;
struct sched_enter_event_hook;
struct sched_exit_event_hook;
struct thread_signal_event_hook;
struct context_switch_event_hook;

typedef void (*TU_EVENT_CALLBACK)(struct time_update_event_hook *);

typedef void (*S_EN_EVENT_CALLBACK)(struct sched_enter_event_hook *);

typedef void (*S_EX_EVENT_CALLBACK)(struct sched_exit_event_hook *);

typedef void (*T_SIG_EVENT_CALLBACK)(struct thread_signal_event_hook *);

typedef void (*C_SWTCH_EVENT_CALLBACK)(struct context_switch_event_hook*);

typedef enum {
  TIME_UPDATE_EVENT = 0,
  SCHEDULER_ENTRY_EVENT,
  SCHEDULER_EXIT_EVENT,
  THREAD_SIGNAL_EVENT,
  CONTEXT_SWITCH_EVENT
} EVENT_TYPE;

struct time_update_event_hook create_time_update_event_hook(size_t ticks);
struct sched_enter_event_hook create_sched_enter_event_hook();
struct sched_exit_event_hook create_sched_exit_event_hook();
struct thread_signal_event_hook create_thread_signal_event_hook(struct thread *thread);
struct context_switch_event_hook create_context_switch_event_hook(struct thread *thread);

ANIVA_STATUS call_event(EVENT_TYPE type, void *hook);

extern void init_global_kevents();

#endif

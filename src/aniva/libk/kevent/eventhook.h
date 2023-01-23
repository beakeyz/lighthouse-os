#ifndef __ANIVA_EVENTHOOK__
#define __ANIVA_EVENTHOOK__
#include <libk/stddef.h>

struct thread;
struct registers;

struct time_update_event_hook {
  struct registers *registers;
  size_t ticks;
};

struct sched_enter_event_hook {
};

struct sched_exit_event_hook {
};

struct thread_signal_event_hook {
  struct thread* thread;
};

struct context_switch_event_hook {
  struct thread* thread;
};

#endif // !__ANIVA_EVENTHOOK__

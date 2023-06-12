#include "idle.h"
#include "sched/scheduler.h"


void generic_proc_idle () {

  for (;;) {
    scheduler_yield();
  }
}

#include "core.h"
#include "dev/debug/serial.h"
#include <time/pit.h>
#include <sched/scheduler.h>

static TICK_TYPE s_kernel_tick_type;

void init_timer_system() {

  set_kernel_ticker_type(PIT);

  //register_global_kevent(TIME_UPDATE_EVENT, s_core_timer_callback);
}

void set_kernel_ticker_type(TICK_TYPE type) {
  if (s_kernel_tick_type == type) {
    return;
  }
  // FIXME: extend
  switch (type) {
    case PIT:
      init_and_install_pit();
      break;
    case APIC_TIMER:
      uninstall_pit();
      break;
    // FIXME: are these two methods viable?
    case HPET:
    case RTC:
      break;
    default:
      break;
  }
  s_kernel_tick_type = type;
}

size_t get_system_ticks() {
  switch (s_kernel_tick_type) {
    case PIT:
      return get_pit_ticks();
    default:
      break;
  }
  return 0;
}

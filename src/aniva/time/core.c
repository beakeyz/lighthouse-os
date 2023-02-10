#include "core.h"
#include <libk/kevent/eventhook.h>
#include <libk/kevent/eventregister.h>
#include "libk/kevent/core.h"
#include "dev/debug/serial.h"
#include <time/pit.h>
#include <sched/scheduler.h>

static TICK_TYPE s_kernel_tick_type;
static TU_EVENT_CALLBACK s_core_timer_callback;
static void __core_timer_callback(struct time_update_event_hook* hook);

void init_timer_system() {
  s_core_timer_callback = __core_timer_callback;

  set_kernel_ticker_type(PIT);

  register_global_kevent(TIME_UPDATE_EVENT, s_core_timer_callback);
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

void __core_timer_callback(struct time_update_event_hook* hook) {
  // TODO:
}
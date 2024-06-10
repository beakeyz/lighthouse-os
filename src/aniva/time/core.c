#include "core.h"
#include <sched/scheduler.h>
#include <time/pit.h>

static TICK_TYPE s_kernel_tick_type;

/*!
 * @brief: Initialize the timer subsystem
 *
 * This happens before we can load an actual driver to do this kind of shit, so we'll need to make sure
 * this code is able to be used both before and after we've loaded any timekeeping drivers
 */
void init_timer_system()
{
    s_kernel_tick_type = UNSET;

    set_kernel_ticker_type(PIT);
}

void set_kernel_ticker_type(TICK_TYPE type)
{
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

size_t get_system_ticks()
{
    switch (s_kernel_tick_type) {
    case PIT:
        return get_pit_ticks();
    default:
        break;
    }
    return 0;
}

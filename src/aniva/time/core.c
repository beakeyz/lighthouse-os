#include "core.h"
#include "entry/entry.h"
#include "irq/interrupts.h"
#include "libk/flow/error.h"
#include "time/apic.h"
#include <sched/scheduler.h>
#include <time/pit.h>

/*
 * There can only be one active tick-source at a time,
 * but we can access all chips, if they are present on the
 * system
 */
static time_chip_t* _active_ticker_chip;
/*
 * Array of chip drivers, indexed by type
 */
static time_chip_t* _system_chip_drivers[TICK_TYPE_COUNT] = {
    [UNSET] = NULL,
    [PIT] = NULL,
    [APIC_TIMER] = NULL,
    [HPET] = NULL,
    [RTC] = NULL,
};

static size_t _system_ticks;

/*!
 * @brief: Get the current system tick count
 */
size_t time_get_system_ticks()
{
    return _system_ticks;
}

/*!
 * @brief: Get the tick source
 */
int time_get_system_tick_type(TICK_TYPE* type)
{
    if (!type)
        return -KERR_INVAL;

    if (!_active_ticker_chip)
        *type = UNSET;
    else
        *type = _active_ticker_chip->type;

    return 0;
}

int time_register_chip(struct time_chip* chip)
{
    if (!chip)
        return -1;

    /* Register the chip indexed by it's type */
    _system_chip_drivers[chip->type] = chip;
    return 0;
}

/*!
 * @brief: Called by chip drivers when they get a tick
 */
int __do_tick(struct time_chip* chip, registers_t* regs, size_t delta)
{
    scheduler_t* this;

    _system_ticks += delta;

    /* Idk what happend here lmao */
    if (!delta)
        return 0;

    /* NOTE: we store the current processor structure in the GS register */
    this = get_current_scheduler();

    if (this && this->f_tick)
        this->f_tick(regs);

    return 0;
}

/*!
 * @brief: Disable a time chip and mark it as such
 *
 * Calls the disable function implemented by the chip driver
 */
static int _timer_chip_disable(time_chip_t* chip)
{
    int error;

    if (!chip)
        return -KERR_INVAL;

    /* Already disabled */
    if ((chip->flags & TIME_CHIP_FLAG_ENABLED) != TIME_CHIP_FLAG_ENABLED)
        return 0;

    error = chip->f_disable(chip);

    if (error)
        return error;

    chip->flags &= ~TIME_CHIP_FLAG_ENABLED;
    return error;
}

/*!
 * @brief: Enable a time chip and mark it as such
 *
 * Calls the enable function implemented by the chip driver
 */
static int _timer_chip_enable(time_chip_t* chip)
{
    int error;

    if (!chip)
        return -KERR_INVAL;

    /* Already enabled */
    if ((chip->flags & TIME_CHIP_FLAG_ENABLED) == TIME_CHIP_FLAG_ENABLED)
        return 0;

    error = chip->f_enable(chip);

    if (error)
        return error;

    chip->flags |= TIME_CHIP_FLAG_ENABLED;
    return error;
}

/*!
 * @brief: Mark a chip driver as active
 *
 * Disables the current chip if it is present and enables the target chip
 */
static int _activate_timer_chip(time_chip_t* chip)
{
    int error;

    /* These functions must be present on a time chip driver */
    if (!chip || !chip->f_enable || !chip->f_disable)
        return -KERR_INVAL;

    error = 0;

    /* Disable current active chip */
    if (_active_ticker_chip)
        error = _timer_chip_disable(_active_ticker_chip);

    if (error)
        return error;

    return _timer_chip_enable(chip);
}

static void _timer_init_apic_timers()
{
    int error;

    /* Try to enable the APIC timer */
    error = _activate_timer_chip(_system_chip_drivers[APIC_TIMER]);

    if (!error)
        return;

    /* Shit, try to enable HPET as a fallback */
    error = _activate_timer_chip(_system_chip_drivers[HPET]);

    if (!error)
        return;

    /* Shit^2, try to enable RTC as a second fallback */
    error = _activate_timer_chip(_system_chip_drivers[RTC]);

    if (!error)
        return;

    /* As a last ditch effort, try to enable PIT =/ */
    error = _activate_timer_chip(_system_chip_drivers[PIT]);

    /* Fuck */
    ASSERT_MSG(error == 0, "Failed to initialize a tick timer");
}

/*!
 * @brief: Initialize timers, when using regular PIC
 *
 * In this case we can probably use the PIT driver, since these two
 * devices are quite often found together on a system
 */
static void _timer_init_pic_timers()
{
    (void)_activate_timer_chip(_system_chip_drivers[PIT]);
}

/*!
 * @brief: Initialize the timer subsystem
 *
 * This happens before we can load an actual driver to do this kind of shit, so we'll need to make sure
 * this code is able to be used both before and after we've loaded any timekeeping drivers
 */
void init_timer_system()
{
    _system_ticks = 0;
    _active_ticker_chip = nullptr;

    /* Initialize all the chip drivers */
    init_pit_chip_driver();
    init_apic_chip_driver();

    // if (g_system_info.irq_chip_type == IRQ_CHIPTYPE_APIC)
    // return _timer_init_apic_timers();

    return _timer_init_pic_timers();
}

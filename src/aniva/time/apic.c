#include "apic.h"
#include "time/core.h"

static int apic_timer_enable(time_chip_t* chip, u32 tps)
{
    return 0;
}

static int apic_timer_disable(time_chip_t* chip)
{
    return 0;
}

static int apic_timer_probe(time_chip_t* chip)
{
    return 0;
}

static time_chip_t apic_chip = {
    .type = APIC_TIMER,
    .flags = NULL,
    .f_enable = apic_timer_enable,
    .f_disable = apic_timer_disable,
    .f_probe = apic_timer_probe,
    .f_get_datetime = NULL,
};

void init_apic_chip_driver()
{
    time_register_chip(&apic_chip);
}

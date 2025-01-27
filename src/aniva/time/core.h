#ifndef __ANIVA_TIME_CORE__
#define __ANIVA_TIME_CORE__

#include "system/processor/registers.h"

/* from <time.h> */
struct tm;

/*
 * The aniva timekeeping subsystem
 *
 * This shit is waaay to bareboner rn, so we'll need to expand on this to add support for multiple
 * kinds of timekeeping chips and types. For example, there are different sources where we can get
 * either ticks, or straight time data from on a single system, so choosing which type and which
 * sources to use, is pretty non-trivial. There are these types of 'time' formats that the system
 * will be dealing with:
 *  - ticks (periodic tick events, vital for things like process scheduling: APIC timer, PIT, ect.)
 *  - global time sources (CMOS, RTC, etc.)
 *
 * We don't need to upgrade right now, so this is a massive TODO
 */

/* A good target for fast machines might be 1000 or more */
#define TARGET_TPS 100

typedef enum TICK_TYPE {
    UNSET = 0,
    PIT,
    APIC_TIMER,
    HPET,
    RTC,

    TICK_TYPE_COUNT
} TICK_TYPE;

#define TIME_CHIP_FLAG_DATETIME 0x00000001
#define TIME_CHIP_FLAG_ENABLED 0x00000002
#define TIME_CHIP_FLAG_PRESENT 0x00000004

typedef struct system_time {
    u64 s_since_boot;
    u64 ms_since_last_s;
    u64 us_since_last_ms;

    struct tm* boot_datetime;
} system_time_t;

typedef struct time_chip {
    TICK_TYPE type;
    uint32_t flags;

    int (*f_enable)(struct time_chip* chip, u32 tps);
    int (*f_disable)(struct time_chip* chip);
    int (*f_probe)(struct time_chip* chip);
    int (*f_get_datetime)(struct time_chip* chip, struct tm* btime);
    int (*f_get_systemtime)(struct time_chip* chip, system_time_t* btime);
    int (*f_get_ticks_per_s)(struct time_chip* chip);
    int (*f_set_ticks_per_s)(struct time_chip* chip);
} time_chip_t;

void init_timer_system();

size_t time_get_system_ticks();
int time_get_system_time(system_time_t* btime);
int time_get_system_tick_type(TICK_TYPE* type);

int time_register_chip(struct time_chip* chip);

/* Called by chip drivers, implemented by the time core */
int __do_tick(struct time_chip* chip, registers_t* regs, size_t delta);

#endif // !__ANIVA_TIME_CORE__

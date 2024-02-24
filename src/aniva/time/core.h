#ifndef __ANIVA_TIME_CORE__
#define __ANIVA_TIME_CORE__

#include "libk/stddef.h"

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
#define TARGET_TPS 500

typedef enum {
  UNSET = 0,
  PIT,
  APIC_TIMER,
  HPET,
  RTC
} TICK_TYPE;

void init_timer_system();
void set_kernel_ticker_type(TICK_TYPE type);
size_t get_system_ticks();

#endif // !__ANIVA_TIME_CORE__

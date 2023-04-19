#ifndef __ANIVA_TIME_CORE__
#define __ANIVA_TIME_CORE__

#include "libk/stddef.h"

#define TARGET_TPS 1000 

typedef enum {
  UNSET,
  PIT,
  APIC_TIMER,
  HPET,
  RTC
} TICK_TYPE;

void init_timer_system();
void set_kernel_ticker_type(TICK_TYPE type);
size_t get_system_ticks();

#endif // !__ANIVA_TIME_CORE__

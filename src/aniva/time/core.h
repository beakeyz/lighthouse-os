#ifndef __ANIVA_TIME_CORE__
#define __ANIVA_TIME_CORE__

#define TARGET_TPS 250

typedef enum {
  UNSET,
  PIT,
  APIC_TIMER,
  HPET,
  RTC
} TICK_TYPE;

void init_timer_system();
void set_kernel_ticker_type(TICK_TYPE type);

#endif // !__ANIVA_TIME_CORE__

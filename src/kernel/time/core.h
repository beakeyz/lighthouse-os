#ifndef __LIGHT_TIME_CORE__
#define __LIGHT_TIME_CORE__

#define TARGET_TPS 250

typedef enum {
  PIT,
  APIC_TIMER,
  HPET,
  RTC
} TIMING_TYPE;

#endif // !__LIGHT_TIME_CORE__

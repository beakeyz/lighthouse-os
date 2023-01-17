#ifndef __ANIVA_TIME_CORE__
#define __ANIVA_TIME_CORE__

#define TARGET_TPS 250

typedef enum {
  PIT,
  APIC_TIMER,
  HPET,
  RTC
} TIMING_TYPE;

#endif // !__ANIVA_TIME_CORE__

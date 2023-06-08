#ifndef __MSR__
#define __MSR__
#include <libk/stddef.h>

#define MSR_EFER        (0xC0000080)

#define MSR_EFER_SCE    (1 << 0)
#define MSR_EFER_LME    (1 << 8)
#define MSR_EFER_LMA    (1 << 10)
#define MSR_EFER_NXE    (1 << 11)
#define MSR_EFER_SVME   (1 << 12)
#define MSR_EFER_LMSLE  (1 << 13)
#define MSR_EFER_FFXSR  (1 << 14)
#define MSR_EFER_TCE   (1 << 15)

#define MSR_STAR        (0xC0000081)
#define MSR_LSTAR       (0xC0000082)
#define MSR_SFMASK      (0xC0000084)

#define MSR_FS_BASE     (0xC0000100)
#define MSR_GS_BASE     (0xC0000101)

// TODO:
ALWAYS_INLINE uint64_t rdmsr(uint32_t address) {
  uint32_t low, high;
  asm volatile("rdmsr"
               : "=a"(low), "=d"(high)
               : "c"(address));
  return ((uintptr_t)high << 32) | low;
}

ALWAYS_INLINE void wrmsr(uint32_t address, uint64_t value) {
  uint32_t low = value & 0xffffffff;
  uint32_t high = value >> 32;
  asm volatile("wrmsr" ::"a"(low), "d"(high), "c"(address));
}

ALWAYS_INLINE void wrmsr_ex(uint32_t address, uint32_t low, uint32_t high) {
  asm volatile("wrmsr" ::"a"(low), "d"(high), "c"(address));
}

#endif

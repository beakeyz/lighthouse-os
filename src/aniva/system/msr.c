#include "msr.h"

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

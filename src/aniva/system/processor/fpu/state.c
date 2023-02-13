#include "state.h"
#include "system/processor/processor.h"

// TODO: fix fxsave throwing an exception...

FpuState standard_fpu_state;

void save_fpu_state(FpuState* buffer) {
  Processor_t *current = get_current_processor();
  bool avx =  processor_has(&current->m_info, X86_FEATURE_XSAVE) && processor_has(&current->m_info, X86_FEATURE_AVX);
  bool fxsr = processor_has(&current->m_info, X86_FEATURE_FXSR);

  if (avx) {
    println("xsave");
    asm volatile("xsave %0\n"
      : "=m"(*buffer)
      : "a"(STATIC_CAST(uint32_t ,(AVX | SSE | X87))), "d"(0u));
  //} else if (fxsr) {
  //  asm volatile("fxsave %0"
  //    : "=m"(*buffer));
  } else {
    asm volatile ("fnsave %0" : "=m"(*buffer));
  }
}

void store_fpu_state(FpuState* buffer) {
  Processor_t *current = get_current_processor();

  bool avx =  processor_has(&current->m_info, X86_FEATURE_XSAVE) && processor_has(&current->m_info, X86_FEATURE_AVX);
  bool fxsr = processor_has(&current->m_info, X86_FEATURE_FXSR);

  if (avx) {
    println("xrstor");
    asm volatile("xrstor %0" ::"m"(*buffer), "a"(STATIC_CAST(uint32_t ,(AVX | SSE | X87))), "d"(0u));
  //} else if (fxsr) {
  //  asm volatile("fxrstor %0" ::"m"(*buffer));
  } else {
    asm volatile ("frstor %0" :: "m"(*buffer));
  }
}
#include "state.h"
#include "dev/debug/serial.h"
#include "system/processor/processor.h"

// TODO: fix fxsave throwing an exception...

FpuState standard_fpu_state;

void save_fpu_state(FpuState* buffer)
{
    processor_t* current = get_current_processor();

    bool avx = processor_has(&current->m_info, X86_FEATURE_XSAVE) && processor_has(&current->m_info, X86_FEATURE_AVX);

    if (avx) {
        asm volatile("xsave %0\n"
                     : "=m"(*buffer)
                     : "a"(STATIC_CAST(uint32_t, (AVX | SSE | X87))), "d"(0u));
    } else {
        asm volatile("fnsave %0" : "=m"(*buffer));
    }
}

void store_fpu_state(FpuState* buffer)
{
    processor_t* current = get_current_processor();

    bool avx = processor_has(&current->m_info, X86_FEATURE_XSAVE) && processor_has(&current->m_info, X86_FEATURE_AVX);

    if (avx) {
        asm volatile("xrstor %0" ::"m"(*buffer), "a"(STATIC_CAST(uint32_t, (AVX | SSE | X87))), "d"(0u));
    } else {
        asm volatile("frstor %0" ::"m"(*buffer));
    }
}

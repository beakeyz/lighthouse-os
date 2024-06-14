#include "state.h"
#include "system/processor/processor.h"

// TODO: fix fxsave throwing an exception...

FpuState standard_fpu_state;

void save_fpu_state(FpuState* buffer)
{
    processor_t* current = get_current_processor();

    if ((current->m_flags & PROCESSOR_FLAG_XSAVE) == PROCESSOR_FLAG_XSAVE) {
        asm volatile("xsave %0\n"
                     : "=m"(*buffer)
                     : "a"(STATIC_CAST(uint32_t, (AVX | SSE | X87))), "d"(0u));
    } else {
        asm volatile("fnsave %0" : "=m"(*buffer));
    }
}

void load_fpu_state(FpuState* buffer)
{
    processor_t* current = get_current_processor();

    if ((current->m_flags & PROCESSOR_FLAG_XSAVE) == PROCESSOR_FLAG_XSAVE) {
        asm volatile("xrstor %0" ::"m"(*buffer), "a"(STATIC_CAST(uint32_t, (AVX | SSE | X87))), "d"(0u));
    } else {
        asm volatile("frstor %0" ::"m"(*buffer));
    }
}

#include "faults.h"

enum FAULT_RESULT fpu_err_handler(const aniva_fault_t* fault, registers_t* regs)
{
    return FR_FATAL;
}

enum FAULT_RESULT x87_fpu_err_handler(const aniva_fault_t* fault, registers_t* regs)
{
    return FR_FATAL;
}

enum FAULT_RESULT simd_fpu_err_handler(const aniva_fault_t* fault, registers_t* regs)
{
    return FR_FATAL;
}

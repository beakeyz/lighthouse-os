#include "faults.h"

enum FAULT_RESULT alignment_handler(const aniva_fault_t* fault, registers_t* regs)
{
    return FR_FATAL;
}

enum FAULT_RESULT seg_overrun_handler(const aniva_fault_t* fault, registers_t* regs)
{
    return FR_FATAL;
}

enum FAULT_RESULT boundrange_handler(const aniva_fault_t* fault, registers_t* regs)
{
    return FR_FATAL;
}

enum FAULT_RESULT overflow_handler(const aniva_fault_t* fault, registers_t* regs)
{
    return FR_FATAL;
}

#include "faults.h"
#include "libk/flow/error.h"

enum FAULT_RESULT doublefault_handler(const aniva_fault_t* fault, registers_t* regs)
{
    return FR_FATAL;
}

enum FAULT_RESULT unknown_err_handler(const aniva_fault_t* fault, registers_t* regs)
{
    return FR_FATAL;
}


#include "irq/faults/faults.h"
#include "libk/flow/error.h"

enum FAULT_RESULT gpf_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("GPF_HANDLER");
}

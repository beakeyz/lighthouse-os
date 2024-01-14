#include "faults.h"

enum FAULT_RESULT machine_chk_handler(const aniva_fault_t* fault, registers_t* regs)
{
  return FR_FATAL;
}

enum FAULT_RESULT virt_handler(const aniva_fault_t* fault, registers_t* regs)
{
  return FR_FATAL;
}

enum FAULT_RESULT control_prot_handler(const aniva_fault_t* fault, registers_t* regs)
{
  return FR_FATAL;
}

enum FAULT_RESULT hyprv_inj_handler(const aniva_fault_t* fault, registers_t* regs)
{
  return FR_FATAL;
}

enum FAULT_RESULT vmm_com_handler(const aniva_fault_t* fault, registers_t* regs)
{
  return FR_FATAL;
}

enum FAULT_RESULT security_fault_handler(const aniva_fault_t* fault, registers_t* regs)
{
  return FR_FATAL;
}


#include "sys_exec.h"
#include "LibSys/syscall.h"
#include "libk/flow/error.h"

uintptr_t sys_exec(char __user* cmd, size_t cmd_len)
{

  kernel_panic("Reached a sys_exec syscall");

  return SYS_INV;
}

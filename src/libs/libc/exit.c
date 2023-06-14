
#include "LibSys/syscall.h"
#include "LibSys/system.h"

static void __attribute__((noreturn)) halt(void)
{
  for (;;){}
}

void exit(uintptr_t result) {

  /* TODO: Uninitialize libraries */

  /* Hard exit */
  syscall_1(SYSID_EXIT, result);

  /* We should never reach this point */
  halt();
}

#include "sys_exit.h"
#include "libk/error.h"

uintptr_t sys_exit_handler(uintptr_t code) {
  kernel_panic("TODO: exit a process");
}

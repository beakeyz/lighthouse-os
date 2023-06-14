
#include <LibSys/system.h>

int Main() {

  syscall_result_t result = syscall_x(0, SYS_5ARG, 0, 1, 2, 3, 4);

  return 999;
}


#include <LibSys/system.h>

int Main() {

  syscall_result_t result = syscall_5(0, 0, 1, 2, 3, 4);

  return 999;
}

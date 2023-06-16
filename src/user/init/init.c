
#include <LibSys/system.h>
#include <LibSys/syscall.h>

int Main() {

  syscall_result_t result = syscall_5(SYSID_OPEN, 0, 1, 2, 3, 4);

  return 999;
}

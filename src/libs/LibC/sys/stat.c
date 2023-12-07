#include "stat.h"
#include "LibSys/syscall.h"
#include <LibSys/system.h>

int mkdir(const char *pathname, mode_t mode)
{
  return syscall_2(SYSID_CREATE_DIR, (uint64_t)pathname, mode);
}

#include "stat.h"
#include <lightos/system.h>

int mkdir(const char* pathname, mode_t mode)
{
    // return syscall_2(SYSID_CREATE_DIR, (uint64_t)pathname, mode);
    return 0;
}

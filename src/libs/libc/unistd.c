#include "unistd.h"
#include "lightos/syscall.h"
#include "lightos/system.h"
#include "stdlib.h"
#include "time.h"

uint32_t sleep(uint32_t seconds)
{
    syscall_1(SYSID_SLEEP, seconds * 1000);
    return 0;
}

uint32_t usleep(uint32_t useconds)
{
    syscall_1(SYSID_SLEEP, useconds / 1000);
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem)
{
    exit_noimpl("nanosleep");
    return 0;
}

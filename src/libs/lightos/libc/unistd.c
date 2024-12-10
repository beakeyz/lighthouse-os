#include "unistd.h"
#include "lightos/syscall.h"
#include "stdlib.h"
#include "time.h"

uint32_t sleep(uint32_t seconds)
{
    sys_sleep(seconds * 1000);
    return 0;
}

uint32_t usleep(uint32_t useconds)
{
    sys_sleep(useconds / 1000);
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem)
{
    exit_noimpl("nanosleep");
    return 0;
}

#include "lightos.h"
#include "lightos/proc/cmdline.h"

extern int __init_devices();

int __init_lightos()
{
    int error;

    error = __init_lightos_cmdline();

    if (error)
        return error;

    error = __init_devices();

    return error;
}

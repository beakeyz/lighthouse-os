
#include "lightos.h"
#include "lightos/proc/cmdline.h"

int __init_lightos()
{
    int error;

    error = __init_lightos_cmdline();

    return error;
}

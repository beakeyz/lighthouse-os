
#include "errno.h"
#include "lightos/syscall.h"
#include "lightos/system.h"
#include <stdio.h>

void __attribute__((noreturn)) halt(void)
{
    for (;;) { }
}

void exit(int result)
{

    /* TODO: Uninitialize libraries */

    /* Hard exit */
    syscall_1(SYSID_EXIT, result);

    /* We should never reach this point */
    halt();
}

void exit_noimpl(const char* impl_name)
{
    printf("Not implemented: %s", impl_name);
    exit(ENOIMPL);
}

/*
 * idk what abort is supposed to do lmao
 */
void abort(void)
{
    exit(-1);
}

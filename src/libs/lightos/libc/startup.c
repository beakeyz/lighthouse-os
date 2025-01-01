#include <mem/memory.h>

#include "lightos/handle_def.h"
#include <lightos/error.h>
#include <lightos/lightos.h>
#include <stdlib.h>
#include <sys/types.h>

/* Generic process entry. No BS */
typedef int (*MainEntry)();
/* Process wants to have a handle to itself */
typedef int (*MainEntry_ex)(HANDLE this);
/* Process is run as a unix/POSIX thingy */
typedef int (*MainEntry_unix)(int argc, char* argv[]);

void lightapp_startup(MainEntry main) __attribute__((used));
void _start(MainEntry main) __attribute__((used));

extern void __attribute__((noreturn)) halt(void);
extern void __init_memalloc(void);
extern void __init_stdio(void);

/*!
 * @brief: Get everything up and running for a standard C application under LightOS
 */
void __init_libc(void)
{
    /* 1) Init userspace libraries */
    /* 1.1 -> init heap for this process (e.g. Ask the kernel for some memory and dump our allocator there) */
    __init_memalloc();
    /* 1.2 -> init posix standard data streams (stdin, stdout, stderr) */
    __init_stdio();
}

/*!
 * Binary entry for statically linked apps
 */
void lightapp_startup(MainEntry main)
{
    process_result_t result;

    if (!main)
        exit(EINVAL);

    /* Initialize LIBC */
    __init_libc();

    /* Walk the dependencies list and initialize LIGHTENTRY functions */

    /* 2) pass appropriate arguments to the program and run it */

    result = main();

    /* 3) Notify the kernel that we have exited so we can be cleaned up */
    exit(result);

    /* 4) Yield */
    halt();
}

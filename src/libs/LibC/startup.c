#include <LibC/mem/memory.h>

#include "lightos/handle_def.h"
#include "lightos/lib/lightos.h"
#include <stdlib.h>
#include <sys/types.h>

/* Generic process entry. No BS */
typedef int (*MainEntry)();
/* Process wants to have a handle to itself */
typedef int (*MainEntry_ex)(HANDLE this);
/* Process is run as a unix/POSIX thingy */
typedef int (*MainEntry_unix)(int argc, char* argv[]);

void lightapp_startup(MainEntry main) __attribute__((used)) ;
void _start(MainEntry main) __attribute__((used));

extern void __attribute__((noreturn)) halt(void);
extern void __init_memalloc(void);
extern void __init_stdio(void);

void __init_libc(void)
{
  /* 1) Init userspace libraries */
  /* 1.1 -> init heap for this process (e.g. Ask the kernel for some memory and dump our allocator there) */
  __init_memalloc();
  /* 1.2 -> init posix standard data streams (stdin, stdout, stderr) */
  __init_stdio();
  /* 1.3 -> lightos userlib initialization */
  __init_lightos();
}

/*!
 * Binary entry for statically linked apps
 */
void lightapp_startup(MainEntry main) 
{
  process_result_t result;

  if (!main)
    exit(ERROR);

  __init_libc();

  /* 2) pass appropriate arguments to the program and run it */

  result = main();

  /* 3) Notify the kernel that we have exited so we can be cleaned up */
  exit(result);

  /* 4) Yield */
  halt();
}

/*!
 * @brief: Dynamic entrypoint for the c runtime library
 */
LIGHTENTRY int libentry()
{
  /* Initialize libc things */
  __init_libc();

  return 0;
}

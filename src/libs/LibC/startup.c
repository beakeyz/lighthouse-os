#include <LibC/mem/memory.h>

#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "lightos/system.h"
#include <stdlib.h>
#include <sys/types.h>

/* Generic process entry. No BS */
typedef int (*MainEntry)();
/* Process wants to have a handle to itself */
typedef int (*MainEntry_ex)(HANDLE this);
/* Process is run as a unix/POSIX thingy */
typedef int (*MainEntry_unix)(int argc, char* argv[]);

void lightapp_startup(MainEntry main) __attribute__((used)) ;
extern void __attribute__((noreturn)) halt(void);

extern void __init_memalloc(void);
extern void __init_stdio(void);

void __init_libc(void)
{
  /* 1) Init userspace libraries */
  /* 1.1 -> init posix standard data streams (stdin, stdout, stderr) */
  /* 1.2 -> init heap for this process (e.g. Ask the kernel for some memory and dump our allocator there) */
  /* 1.2.1 -> we could allow for custom heap creation, which could mean that every process may choose which kind of heap they would like to use */
  /* 1.3 -> attach ourselves to certain events we might need */
  /* 1.4 -> create configuration entries if these don't already exist */
  /* 1.5 -> create indecies for any threads we might create so they can easily be cleaned up */
  /* 1.x -> TODO */
  __init_memalloc();
  __init_stdio();
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

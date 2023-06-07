
#include <LibDef/def.h>

typedef int (*MainEntry)();

__attribute__((used)) void lightapp_startup(MainEntry main);

/*
 * TODO: library initialization for userspace
 */
void lightapp_startup(MainEntry main) {

  /* 1) Init userspace libraries */

  /* 2) pass appropriate arguments to the program and run it */

  /* 3) Notify the kernel that we have exited so we can be cleaned up */

  /* 4) Yield to the kernel */

  uintptr_t ret = main();

  (void)ret;

  for (;;) {}
}

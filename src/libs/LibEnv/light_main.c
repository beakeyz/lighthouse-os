
#include <LibDef/def.h>

typedef int (*MainEntry)();

void lightapp_startup(MainEntry main);


/*
 * TODO: library initialization for userspace
 */
void lightapp_startup(MainEntry main) {

  uintptr_t ret = main();

  for (;;) {}
}

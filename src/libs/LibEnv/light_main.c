
#include <LibDef/def.h>

typedef int (*Entry)();

void lightapp_startup(Entry main);


/*
 * TODO: library initialization for userspace
 */
void lightapp_startup(Entry main) {

  uintptr_t ret = main();
  for (;;) {}
}

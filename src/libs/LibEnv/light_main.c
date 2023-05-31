
#include <LibDef/def.h>

typedef int (*MainEntry)();

__attribute__((used)) void lightapp_startup(MainEntry main);

/*
 * TODO: library initialization for userspace
 */
void lightapp_startup(MainEntry main) {

  for (;;) {}

  uintptr_t ret = main();

}

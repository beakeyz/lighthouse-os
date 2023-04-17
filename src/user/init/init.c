#include <LibStr/str.h>
#include <LibDef/def.h>

int Main() {

  str_t s = "Test";

  uintptr_t a = 69420;

  s += a;

  return a;
}

#include "stdlib.h"
#include <lightos/lib/shared.h>
#include <lightos/handle_def.h>
#include <stdio.h>

/*!
 * @brief: Entry for shared library
 */
LIGHTENTRY int gfxlibentry()
{
  for(;;){}

  printf("Initialized libgfx!\n");
  return 0;
}

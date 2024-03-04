#include <lightos/lib/shared.h>
#include <lightos/handle_def.h>
#include <stdio.h>

/*!
 * @brief: Entry for shared library
 */
LIGHTENTRY int libentry()
{
  printf("Initialized libgfx!\n");
  return 0;
}

#include <lightos/lib/shared.h>
#include <stdio.h>

/*!
 * @brief: Entry for shared library
 */
LIGHTENTRY int gfxlibentry(void)
{
  printf("Initialized gfxlib!\n");
  return 0;
}

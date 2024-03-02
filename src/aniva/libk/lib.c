#include "lib.h"
#include "libk/cmdline/parser.h"
#include "libk/data/hashmap.h"


/*!
 * @brief: Initialize the kernel core library
 *
 * This means we:
 *  - allocate caches for data structures
 *  - initialize the cmdline parser
 *  - ...
 */
void init_libk()
{
  /* Initialize hashmap caching */
  init_hashmap();

  /* Initialize the kernel cmdline parser */
  init_cmdline_parser();
}

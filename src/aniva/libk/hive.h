#ifndef __ANIVA_HIVE__
#define __ANIVA_HIVE__
#include <libk/stddef.h>

/*
 * experimental data structure: the hive works by combining a height-map with a two-dimensional list.
 * every entry holds 6 pointers to its neighbors, its height in the grid, and some data. The height value
 * every entry holds represents the relevance of this entry in the hive, where a high height means greater
 * importance.
 *
 * One issue is that we need this to be efficient, which will be a challenge and maybe even impossible...
 */

typedef struct hive_entry {
  struct hive_entry* m_entries[6];
  size_t m_height;
  void* m_data;
} hive_entry_t;

typedef struct {

} hive_t;

#endif //__ANIVA_HIVE__

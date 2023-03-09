#ifndef __ANIVA_HIVE__
#define __ANIVA_HIVE__
#include "libk/linkedlist.h"
#include <libk/stddef.h>
#include <libk/error.h>

/*
 * experimental data structure: the hive works by combining a height-map with a two-dimensional list.
 * every entry holds 6 pointers to its neighbors, its height in the grid, and some data. The height value
 * every entry holds represents the relevance of this entry in the hive, where a high height means greater
 * importance.
 *
 * One issue is that we need this to be efficient, which will be a challenge and maybe even impossible...
 * 
 *     hive0 -> hive0_entry1 -> hive1 -> hive1_entry1
 *              |   |               \-> hive1_entry2
 *          entry1  entry2
 */

struct hive_entry;
struct hive;

typedef enum HIVE_ENTRY_TYPE {
  HIVE_ENTRY_TYPE_DATA = 0,
  HIVE_ENTRY_TYPE_HOLE
} HIVE_ENTRY_TYPE_t;

#define HIVE_PART_SEPERATOR '.'

typedef char* hive_url_part_t;

/*
 * An entry can either be a DATA_ENTRY (which holds a pointer), or an HOLE_ENTRY (which holds a pointer to another hive)
 */
typedef struct hive_entry {

  HIVE_ENTRY_TYPE_t m_type;

  union {
    struct {
      void* m_data;
    };
    struct {
      struct hive* m_hole;
    };
  };

  hive_url_part_t m_entry_part;
} hive_entry_t;

typedef struct hive {
  list_t *m_entries;
  size_t m_hole_count;

  hive_url_part_t m_url_part;
} hive_t;

/*
 * Allocate and init a hive struct
 */
hive_t *create_hive(hive_url_part_t root_part);

/*
 * Add some data to a hive
 */
ErrorOrPtr hive_add_entry(hive_t* root, void* data, const char* path);

/*
 * Follows the path and inserts a hole to another
 * hive there
 */
ErrorOrPtr hive_add_hole(hive_t* root, const char* path);

/*
 * Follows the path and inserts holes where they are needed
 */
ErrorOrPtr hive_add_holes(hive_t* root, const char* path);

/*
 * Find data in the hive based on the path
 */
void* hive_get(hive_t* root, const char* path);

/*
 * Remove an entry from the hive
 */
ErrorOrPtr hive_remove(hive_t* root, void* data);

/*
 * Remove an entry using its path. this only
 * removes the outermost entry
 */
ErrorOrPtr hive_remove_path(hive_t* root, const char* path);

/*
 * Find the path of an entry
 */
const char* hive_get_path(hive_t* root, void* data);

/*
 * Checks wether this pointer is contained in the hive
 */
bool hive_contains(hive_t* root, void* data);

/*
 * Walk the complete hive and call the itterate_fn on each entry
 *
 * --this is a recursive function--
 */
ErrorOrPtr hive_walk(hive_t* root, bool (*itterate_fn)(void* hive, void* data));

static ALWAYS_INLINE bool hive_entry_is_hole(hive_entry_t* entry) {
  return (entry->m_type == HIVE_ENTRY_TYPE_HOLE && entry->m_hole != nullptr);
}

#endif //__ANIVA_HIVE__

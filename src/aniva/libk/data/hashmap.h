#ifndef __ANIVA_LIBK_HASHMAP__
#define __ANIVA_LIBK_HASHMAP__

#include "libk/flow/error.h"
#include <libk/stddef.h>

/*
 * This implementation of a hashmap uses closed addressing (so no linear probing)
 * which means that keys that result in duplicate entries are stored in a linked list 
 */

typedef void* hashmap_key_t;
typedef void* hashmap_value_t;

struct hashmap_entry;

#define HASHMAP_FLAG_SK         (0x00000001) /* Does this hashmap use strings for keys? */
#define HASHMAP_FLAG_CA         (0x00000002) /* Does this hashmap use closed addressing? (FIXME/TODO: open addressing is still unimplemented) */
#define HASHMAP_FLAG_FS         (0x00000004) /* Is this hashmap fixed in size? */

typedef struct __hashmap {
  size_t m_max_entries;
  size_t m_size;
  size_t m_total_size;

  uintptr_t (*f_hash_func)(hashmap_key_t data);

  /* FIXME: this takes up 8 bytes instead of 4... */
  uint32_t m_flags;

  hashmap_value_t m_list[];
} hashmap_t;

typedef ErrorOrPtr (*hashmap_itterate_fn_t) (hashmap_value_t value, uint64_t arg0, uint64_t arg1);

void init_hashmap();

hashmap_t* create_hashmap(size_t max_entries, uint32_t flags);
void destroy_hashmap(hashmap_t* map);

ErrorOrPtr hashmap_itterate(hashmap_t* map, hashmap_itterate_fn_t fn, uint64_t arg0, uint64_t arg1);

ErrorOrPtr hashmap_put(hashmap_t* map, hashmap_key_t key, hashmap_value_t value);
ErrorOrPtr hashmap_set(hashmap_t* map, hashmap_key_t key, hashmap_value_t value);
hashmap_value_t hashmap_remove(hashmap_t* map, hashmap_key_t key);
hashmap_value_t hashmap_get(hashmap_t* map, hashmap_key_t key);
int hashmap_to_array(hashmap_t* map, void*** array_ptr, size_t* size_ptr);

static inline bool hashmap_has(hashmap_t* map, hashmap_key_t key) {
  return (hashmap_get(map, key) != nullptr);
}

#endif // !__ANIVA_LIBK_HASHMAP__

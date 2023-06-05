#ifndef __ANIVA_LIBK_HASHMAP__
#define __ANIVA_LIBK_HASHMAP__

#include "libk/error.h"
#include <libk/stddef.h>

/*
 * This implementation of a hashmap uses closed addressing (so no linear probing)
 * which means that keys that result in duplicate entries are stored in a linked list 
 */

typedef void* hashmap_key_t;
typedef void* hashmap_value_t;

typedef struct __hashmap_entry {

  /* The actual value stored at this address */
  hashmap_value_t m_value;

  uintptr_t m_hash;

  struct __hashmap_entry* m_next;

} hashmap_entry_t;

#define HASHMAP_FLAG_SK         (0x00000001) /* Does this hashmap use strings for keys? */
#define HASHMAP_FLAG_CA         (0x00000002) /* Does this hashmap use closed addressing? (FIXME/TODO: open addressing is still unimplemented) */
#define HASHMAP_FLAG_FS         (0x00000004) /* Is this hashmap fixed in size? */

typedef struct __hashmap {

  size_t m_max_size;
  size_t m_size;

  uintptr_t (*f_hash_func)(hashmap_key_t data);

  /* FIXME: this takes up 8 bytes instead of 4... */
  uint32_t m_flags;

  hashmap_value_t m_list[];
} hashmap_t;

hashmap_t* create_hashmap(size_t max_size, uint32_t flags);

ErrorOrPtr hashmap_put(hashmap_t* map, hashmap_key_t key, hashmap_value_t value);
ErrorOrPtr hashmap_set(hashmap_t* map, hashmap_key_t key, hashmap_value_t value);
hashmap_value_t hashmap_get(hashmap_t* map, hashmap_key_t key);

bool hashmap_has(hashmap_t* map, hashmap_key_t key);

#endif // !__ANIVA_LIBK_HASHMAP__

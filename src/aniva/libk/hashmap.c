#include "hashmap.h"
#include "libk/error.h"
#include "mem/heap.h"

hashmap_t* create_hashmap(size_t max_size, uint32_t flags) {

  hashmap_t* ret;
  size_t hashmap_size;

  if (!max_size)
    return nullptr;

  hashmap_size = sizeof(hashmap_t) + (max_size * sizeof(void*));

  ret = kmalloc(hashmap_size);

  memset(ret, 0, hashmap_size);

  ret->m_flags = flags;
  ret->m_size = 0;
  ret->m_max_size = max_size;

  /*
  ret->f_hash_func = nullptr;
  ret->f_hash_func_str = nullptr;
  */

  return ret;
}

ErrorOrPtr hashmap_put(hashmap_t* map, hashmap_key_t key, hashmap_value_t value) {
  return Error();
}

hashmap_value_t hashmap_get(hashmap_t* map, hashmap_key_t key) {
  return nullptr;
}

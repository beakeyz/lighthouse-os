#ifndef __ANIVA_LIBK_HASHMAP__
#define __ANIVA_LIBK_HASHMAP__

#include <libk/stddef.h>

typedef void* hashmap_key_t;
typedef char* hashmap_str_key_t;
typedef void* hashmap_value_t;

#define HASHMAP_FLAG_STR_KEY 0x00000001

typedef struct {

  size_t m_max_size;
  size_t m_size;

  uint32_t m_flags;

  void* m_list;

  uintptr_t (*f_hash_func_str)(hashmap_str_key_t str);
  uintptr_t (*f_hash_func)(hashmap_key_t data);
} hashmap_t;

hashmap_t* create_hashmap(size_t max_size);

void* hashmap_find();

#endif // !__ANIVA_LIBK_HASHMAP__

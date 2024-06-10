#ifndef __ANIVA_VECTOR__
#define __ANIVA_VECTOR__
#include <libk/flow/error.h>
#include <libk/stddef.h>

#define VEC_FLAG_FLEXIBLE (0x0001)
#define VEC_FLAG_NO_DUPLICATES (0x0002)

typedef struct vector {
    size_t m_capacity;
    size_t m_length;
    uint16_t m_flags;
    uint16_t m_entry_size;
    uint32_t m_grow_size;
    uint8_t* m_items;
} vector_t;

#define FOREACH_VEC(vector, data, index) for (uintptr_t data = (uintptr_t) & (vector)->m_items[0], index = 0; index < (vector)->m_length; index++, data = (uintptr_t) & (vector)->m_items[index * (vector)->m_entry_size])

vector_t* create_vector(size_t capacity, uint16_t entry_size, uint32_t flags);

/*
 * allocate a vector with its m_items
 */
void init_vector(vector_t* vec, size_t capacity, uint16_t entry_size, uint32_t flags);

/*
 * free the entries of the vector
 */
void destroy_vector(vector_t* ptr);

/*
 * get an item by its index
 */
void* vector_get(vector_t*, uint32_t);

/*
 * get the index of an item
 */
ErrorOrPtr vector_indexof(vector_t*, void*);

/*
 * add an item
 */
ErrorOrPtr vector_add(vector_t*, void*);

/*
 * remove an item at an index
 */
ErrorOrPtr vector_remove(vector_t*, uint32_t);

#endif //__ANIVA_VECTOR__

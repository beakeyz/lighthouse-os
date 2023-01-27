#ifndef __ANIVA_VECTOR__
#define __ANIVA_VECTOR__
#include <libk/stddef.h>
#include <libk/error.h>
#include "linkedlist.h"

typedef struct vector {
  list_t m_items;
  size_t m_max_capacity;
} vector_t;

/*
 * allocate a vector on the heap
 */
vector_t* create_vector(size_t max_capacity);

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
ANIVA_STATUS vector_add(vector_t*, void*);

/*
 * remove an item
 */
ANIVA_STATUS vector_remove(vector_t*, uint32_t);

/*
 * ensure vector has a certain capacity and expand if we need more
 */
ANIVA_STATUS vector_ensure_capacity(vector_t*, size_t);

#endif //__ANIVA_VECTOR__
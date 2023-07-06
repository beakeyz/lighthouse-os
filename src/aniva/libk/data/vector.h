#ifndef __ANIVA_VECTOR__
#define __ANIVA_VECTOR__
#include <libk/stddef.h>
#include <libk/flow/error.h>
#include "linkedlist.h"

typedef struct vector {
  size_t m_max_capacity;
  size_t m_length;
  void** m_items;
} vector_t;

/*
 * allocate a vector with its m_items
 */
vector_t create_vector(size_t max_capacity);

/*
 * free the entries of the vector
 */
void vector_clean(vector_t* ptr);

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

#endif //__ANIVA_VECTOR__

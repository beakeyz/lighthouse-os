#ifndef __ANIVA_VECTOR__
#define __ANIVA_VECTOR__
#include <libk/stddef.h>
#include <libk/error.h>
#include "linkedlist.h"

typedef struct vector {
  list_t m_items;
  size_t m_max_capacity;
} vector_t;

vector_t* create_vector(size_t max_capacity);

void* vector_get(vector_t*, uint32_t);
ANIVA_STATUS vector_add(vector_t*, void*);
ANIVA_STATUS vector_remove(vector_t*, uint32_t);

#endif //__ANIVA_VECTOR__
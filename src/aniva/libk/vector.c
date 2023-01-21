#include "vector.h"
#include <mem/kmalloc.h>

vector_t* create_vector(size_t max_capacity) {
  vector_t* vec = kmalloc(sizeof(vector_t));
  vec->m_max_capacity = max_capacity;
  vec->m_items.m_length = 0;
  vec->m_items.end = nullptr;
  vec->m_items.end = nullptr;
  return vec;
}

void* vector_get(vector_t* vec, uint32_t index) {
  if (index < vec->m_max_capacity) {
    return list_get(&vec->m_items, index);
  }
  return nullptr;
}

ANIVA_STATUS vector_add(vector_t* vec, void* data) {
  if (vec->m_items.m_length < vec->m_max_capacity) {
    list_append(&vec->m_items, data);
    return ANIVA_SUCCESS;
  }
  return ANIVA_FAIL;
}

ANIVA_STATUS vector_remove(vector_t* vec, uint32_t index) {
  if (vec->m_items.m_length > 0) {
    list_remove(&vec->m_items, index);
    return ANIVA_SUCCESS;
  }
  return ANIVA_FAIL;
}

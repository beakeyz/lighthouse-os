#include "vector.h"
#include <mem/heap.h>
#include <libk/string.h>

vector_t create_vector(size_t max_capacity) {

  vector_t ret = {
    .m_max_capacity = max_capacity,
    .m_length = 0,
    .m_items = kmalloc(sizeof(void*) * max_capacity)
  };

  return ret;
}

void vector_clean(vector_t* ptr) {
  if (ptr != nullptr && ptr->m_items != nullptr) {
    kfree(ptr->m_items);
  }
}

ErrorOrPtr vector_indexof(vector_t* vec, void* value) {

  for (int i = 0; i < vec->m_max_capacity; i++) {
    if (vec->m_items[i] == value) {
      return Success(i);
    }
  }

  return Error();
}

void* vector_get(vector_t* vec, uint32_t index) {
  if (index < vec->m_max_capacity) {
    return vec->m_items[index];
  }
  return nullptr;
}

ANIVA_STATUS vector_add(vector_t* vec, void* data) {
  if (vec->m_length < vec->m_max_capacity) {
    vec->m_items[vec->m_length] = data;
    vec->m_length++;
  }
  return ANIVA_FAIL;
}

ANIVA_STATUS vector_remove(vector_t* vec, uint32_t index) {
  if (vec->m_length > 0) {

    vec->m_items[index] = 0;

    for (uintptr_t i = index; i < vec->m_length-1; i++) {
      vec->m_items[i] = vec->m_items[i+1];
    }

    return ANIVA_SUCCESS;
  }
  return ANIVA_FAIL;
}

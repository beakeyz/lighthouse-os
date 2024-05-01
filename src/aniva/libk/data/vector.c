#include "vector.h"
#include "libk/flow/error.h"
#include <mem/heap.h>
#include <libk/string.h>

void init_vector(vector_t* vec, size_t capacity, uint16_t entry_size, uint32_t flags) {

  if (!vec)
    return;

  vec->m_capacity = capacity;
  vec->m_grow_size = (uint32_t)capacity;
  vec->m_length = 0;
  vec->m_entry_size = entry_size;
  vec->m_flags = flags;
  vec->m_items = kmalloc(entry_size * capacity);

  memset(vec->m_items, 0, entry_size * capacity);
}

vector_t* create_vector(size_t capacity, uint16_t entry_size, uint32_t flags) {
  vector_t* ret;

  if (!capacity)
    return nullptr;

  ret = kmalloc(sizeof(vector_t));

  init_vector(ret, capacity, entry_size, flags);

  return ret;
}

void destroy_vector(vector_t* ptr) {

  if (!ptr || !ptr->m_items)
    return;

  kfree(ptr->m_items);

  memset(ptr, 0, sizeof(vector_t));

  kfree(ptr);
}

/*
 * get a entry at an unchecked index 
 */
static void* __vector_get_at(vector_t* ptr, uint32_t index)
{
  return &ptr->m_items[index * ptr->m_entry_size];
}

static bool __vector_no_duplicates(vector_t* vec) 
{
  return (vec->m_flags & VEC_FLAG_NO_DUPLICATES) == VEC_FLAG_NO_DUPLICATES;
}

static bool __vector_grow(vector_t* ptr)
{
  size_t new_capacity;
  uint8_t* new_entry_list;

  if (!ptr || !ptr->m_capacity)
    return false;

  if (!(ptr->m_flags & VEC_FLAG_FLEXIBLE))
    return false;

  new_capacity = ptr->m_capacity + ptr->m_grow_size;
  new_entry_list = kmalloc(new_capacity * ptr->m_entry_size);

  if (ptr->m_items) {
    memcpy(new_entry_list, ptr->m_items, ptr->m_length * ptr->m_entry_size);
    kfree(ptr->m_items);
  }

  ptr->m_items = new_entry_list;
  ptr->m_capacity = new_capacity;

  return true;
}

ErrorOrPtr vector_indexof(vector_t* vec, void* value) {

  for (int i = 0; i < vec->m_length; i++) {
    if (memcmp(__vector_get_at(vec, i), value, vec->m_entry_size)) {
      return Success(i);
    }
  }

  return Error();
}

void* vector_get(vector_t* vec, uint32_t index) {

  if (!vec || index >= vec->m_length)
    return nullptr;

  return __vector_get_at(vec, index);
}

/*!
 * @brief Add an entry to the end of the vector
 *
 * @returns: Error() with no status on failure, Success() with 
 * the index of the new entry on success
 */
ErrorOrPtr vector_add(vector_t* vec, void* data) 
{
  if (vec->m_length >= vec->m_capacity && !__vector_grow(vec))
    return Error();

  if (__vector_no_duplicates(vec) && !IsError(vector_indexof(vec, data)))
    return Error();

  /* Copy the data */
  memcpy(__vector_get_at(vec, vec->m_length), data, vec->m_entry_size);

  vec->m_length++;

  return Success(vec->m_length-1);
}

/*!
 * @brief Remove an entry form the vector
 *
 * Find the index in the array and shifts every entry after it 
 * by back by one index, so the entry to remove gets overwritten
 *
 * @returns: Error() on failure, Success() when we could find the 
 * entry and it was successfully removed
 */
ErrorOrPtr vector_remove(vector_t* vec, uint32_t index) {
  if (!vec->m_length || index >= vec->m_length)
    return Error();

  for (uintptr_t i = index; i < vec->m_length; i++) {
    if (i+1 == vec->m_length) {
      memset(__vector_get_at(vec, i), 0, vec->m_entry_size);
    }
    memcpy(__vector_get_at(vec, i), __vector_get_at(vec, i+1), vec->m_entry_size);
  }

  vec->m_length--;

  return Success(0);
}

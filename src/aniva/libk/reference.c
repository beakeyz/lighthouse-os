#include "reference.h"
#include "mem/heap.h"

refc_t* create_refc(FuncPtr destructor, void* referenced_handle) {
  refc_t* ret = kmalloc(sizeof(refc_t));

  if (!ret)
    return nullptr;

  ret->m_count = 0;
  ret->m_destructor = destructor;
  ret->m_referenced_handle = referenced_handle;
  return ret;
}

void destroy_refc(refc_t* ref) {
  kfree(ref);
}

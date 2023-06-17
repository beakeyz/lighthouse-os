#include "reference.h"
#include "dev/debug/serial.h"
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

void unref(refc_t* rc) {
  ASSERT_MSG(rc->m_count > 0, "Tried to unreference without first referencing");

  rc->m_count--;

  if (!rc->m_destructor)
    return;

  if (rc->m_count <= 0) {
    rc->m_destructor(rc->m_referenced_handle);
  }
}

#ifndef __ANIVA_LIBK_REFERENCE__
#define __ANIVA_LIBK_REFERENCE__
#include "libk/error.h"
#include "libk/io.h"
#include "mem/heap.h"
#include <libk/stddef.h>
#include <dev/debug/serial.h>

typedef int flat_refc_t;

typedef struct refc {
  flat_refc_t m_count;
  void* m_referenced_handle;

  // called when the refcount hits zero
  void (*m_destructor) (void* handle);
} refc_t;

refc_t* create_refc(FuncPtr destructor, void* referenced_handle);
void destroy_refc(refc_t* ref);

static ALWAYS_INLINE void flat_ref(flat_refc_t* frc_p) {
  flat_refc_t i = *(frc_p)++;
}

static ALWAYS_INLINE void flat_unref(flat_refc_t* frc_p) {
  flat_refc_t i = *(frc_p)--;
  if (i < 0) {
    // bro wtf
    *frc_p = 0;
  }
}

static ALWAYS_INLINE void ref(refc_t* rc) {
  rc->m_count++;
}

static ALWAYS_INLINE void unref(refc_t* rc) {
  ASSERT_MSG(rc->m_count > 0, "Tried to unreference without first referencing");

  rc->m_count--;
  if (rc->m_count <= 0) {
    rc->m_destructor(rc->m_referenced_handle);
  }
}

#endif // !__ANIVA_LIBK_REFERENCE__

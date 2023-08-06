#ifndef __ANIVA_LIBK_REFERENCE__
#define __ANIVA_LIBK_REFERENCE__
#include "libk/flow/error.h"
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

void init_refc(refc_t* ref, FuncPtr destructor, void* handle);
refc_t* create_refc(FuncPtr destructor, void* referenced_handle);
void destroy_refc(refc_t* ref);

static ALWAYS_INLINE bool is_referenced(refc_t* ref) {
  return (ref->m_count != 0);
}

static ALWAYS_INLINE void flat_ref(flat_refc_t* frc_p) {
  *frc_p += 1;
}

static ALWAYS_INLINE void flat_unref(flat_refc_t* frc_p) {
  if (!frc_p || !(*frc_p)) {
    return;
  }
  (*frc_p)--;
}

static ALWAYS_INLINE void ref(refc_t* rc) {
  rc->m_count++;
}

void unref(refc_t* rc);

#endif // !__ANIVA_LIBK_REFERENCE__

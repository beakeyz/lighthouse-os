#ifndef __ANIVA_LIBK_REFERENCE__
#define __ANIVA_LIBK_REFERENCE__
#include "libk/error.h"
#include "mem/heap.h"
#include <libk/stddef.h>
#include <dev/debug/serial.h>

typedef int flat_refc_t;

typedef struct refc {
  flat_refc_t count;

  // called when the refcount hits zero
  FuncPtr* zero_waiters;
  size_t zero_waiters_count;
} refc_t;

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
  rc->count++;
}

static ALWAYS_INLINE void unref(refc_t* rc) {
  rc->count--;
  if (rc->count <= 0) {
    uintptr_t idx = 0;
    FuncPtr ptr = rc->zero_waiters[0];
    while (ptr) {

      ptr();
      
      idx++;
      ptr = rc->zero_waiters[idx];
    }
  }
}

static ALWAYS_INLINE void clean_refc_waiters(refc_t* rc) {
  if (rc->zero_waiters != nullptr && rc->zero_waiters_count > 0) {
    // let's assume that this was kmalloced as one big list
    rc->zero_waiters_count = 0;
    kfree(rc->zero_waiters);
  }
}

#endif // !__ANIVA_LIBK_REFERENCE__

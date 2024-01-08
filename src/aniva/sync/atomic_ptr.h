#ifndef __ANIVA_ATOMIC_PTR__
#define __ANIVA_ATOMIC_PTR__

#include "libk/atomic.h"
#include <libk/stddef.h>

typedef struct {
  volatile _Atomic uintptr_t __val; 
} atomic_ptr_t ;

int init_atomic_ptr(atomic_ptr_t* ptr, uintptr_t value);
atomic_ptr_t* create_atomic_ptr();
atomic_ptr_t* create_atomic_ptr_ex(uintptr_t initial_value);
void destroy_atomic_ptr(atomic_ptr_t* ptr);

uintptr_t atomic_ptr_read(atomic_ptr_t* ptr);
void atomic_ptr_write(atomic_ptr_t* ptr, uintptr_t value);

#endif // !__ANIVA_ATOMIC_PTR__

#ifndef __LIGHT_ATOMIC_PTR__
#define __LIGHT_ATOMIC_PTR__
#include <libk/stddef.h>

typedef union atomic_ptr atomic_ptr_t;

atomic_ptr_t* create_atomic_ptr();
uintptr_t atomic_ptr_load(atomic_ptr_t* ptr);
void atomic_ptr_write(atomic_ptr_t* ptr, uintptr_t value);

#endif // !__LIGHT_ATOMIC_PTR__

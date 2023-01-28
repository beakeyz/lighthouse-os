#ifndef __ANIVA_ATOMIC_PTR__
#define __ANIVA_ATOMIC_PTR__
#include <libk/stddef.h>

typedef union atomic_ptr atomic_ptr_t;

/*
 * create an empty atomic_ptr
 */
atomic_ptr_t* create_atomic_ptr();

/*
 * above function, but with an initial value
 */
atomic_ptr_t* create_atomic_ptr_with_value(uintptr_t initial_value);

uintptr_t atomic_ptr_load(atomic_ptr_t* ptr);
void atomic_ptr_write(atomic_ptr_t* ptr, uintptr_t value);

#endif // !__ANIVA_ATOMIC_PTR__

#ifndef __ANIVA_LIBK_TUPLE__
#define __ANIVA_LIBK_TUPLE__
#include "libk/stddef.h"

// should have the same size
// TODO: tuples of different types
typedef struct tuple {
    uintptr_t one;
    uintptr_t two;
} tuple_t;

typedef struct ptr_tuple {
    void* one;
    void* two;
} ptr_tuple_t;

tuple_t create_tuple_t(uintptr_t one, uintptr_t two);
tuple_t empty_tuple_t();

ptr_tuple_t create_ptr_tuple_t(void* one, void* two);
ptr_tuple_t empty_ptr_tuple_t();

bool compare_tuple(tuple_t* one, tuple_t* two);
bool compare_ptr_tuple(ptr_tuple_t* one, ptr_tuple_t* two);

#endif // !__ANIVA_LIBK_TUPLE__

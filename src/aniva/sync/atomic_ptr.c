#include "atomic_ptr.h"
#include "libk/atomic.h"
#include <mem/heap.h>

int init_atomic_ptr(atomic_ptr_t* ptr, uintptr_t value)
{
    atomic_ptr_write(ptr, value);
    return 0;
}

atomic_ptr_t* create_atomic_ptr()
{
    return create_atomic_ptr_ex(NULL);
}

atomic_ptr_t* create_atomic_ptr_ex(uintptr_t initial_value)
{
    atomic_ptr_t* ptr;

    ptr = kmalloc(sizeof(*ptr));

    if (!ptr)
        return nullptr;

    init_atomic_ptr(ptr, initial_value);
    return ptr;
}

void destroy_atomic_ptr(atomic_ptr_t* ptr)
{
    kfree((void*)ptr);
}

uintptr_t atomic_ptr_read(atomic_ptr_t* ptr)
{
    return atomicLoad_alt((volatile uintptr_t*)&ptr->__val);
}

void atomic_ptr_write(atomic_ptr_t* ptr, uintptr_t value)
{
    atomicStore_alt((volatile uintptr_t*)&ptr->__val, value);
}

#include "reference.h"
#include "dev/debug/serial.h"
#include "mem/heap.h"

void init_refc(refc_t* ref, FuncPtr destructor, void* handle)
{
    if (!ref)
        return;

    ref->m_count = 0;
    ref->m_destructor = destructor;
    ref->m_referenced_handle = handle;
}

refc_t* create_refc(FuncPtr destructor, void* referenced_handle)
{
    refc_t* ret = kmalloc(sizeof(refc_t));

    init_refc(ret, destructor, referenced_handle);

    return ret;
}

void destroy_refc(refc_t* ref)
{
    kfree(ref);
}

void unref(refc_t* rc)
{
    ASSERT_MSG(rc->m_count > 0, "Tried to unreference without first referencing");

    flat_unref(&rc->m_count);

    if (!rc->m_destructor)
        return;

    if (!rc->m_count) {
        /* NOTE: Any good destructor should also destroy its refcount */
        rc->m_destructor(rc->m_referenced_handle);
    }
}

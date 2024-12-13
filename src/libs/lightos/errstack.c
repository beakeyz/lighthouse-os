#include "lightos/errstack.h"
#include "lightos/types.h"
#include <lightos/handle.h>
#include <lightos/lightos.h>

typedef struct errstack {
    /*
     * Every thread gets their own errstack (Currently unused, until we implement thread handles)
     * TODO: Handles to threads
     */
    HANDLE thread;
    /* Enqueue pointer */
    errstack_entry_t** enq;
    /* Base pointer for the stack */
    errstack_entry_t* base;
} errstack_t;

/*!
 * @brief:
 */
error_t __push_lightos_err(error_t error, const char* function, u32 line, const char* desc)
{
    return 0;
}

/*!
 * @brief: Pops the entire error stack, if
 *
 * Used when an error is handled.
 */
error_t __pop_lightos_errstack(error_t error)
{
    return 0;
}

/*!
 * @brief: Called when the errorstack was previously disabled
 *
 * At this point we need to allocate some memory for the stack and initialize that
 */
LEXPORT error_t __lightos_disable_errstack()
{
    return 0;
}

/*!
 *
 */
LEXPORT error_t __lightos_enable_errstack()
{
    return 0;
}

/*!
 * @brief: Initialize the errstack lightos service
 *
 * Called by the lightos library startup code. Here we initialize general stuff and we
 * create the first errstack for the main thread.
 *
 * TODO: Hook into the threads kevent to get callbacks for created threads, so we can create
 * errstacks for them
 */
LEXPORT error_t __init_errstack(void)
{
    return 0;
}

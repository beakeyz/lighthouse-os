#ifndef __LIGHTOS_ERROR_STACK__
#define __LIGHTOS_ERROR_STACK__

/*
 * Experimental concept. Maybe this will turn out to be impossible/impractical xD
 *
 * The idea is to give every thread their own error stack that they can push errors on. If a thread
 * ever 'crashes' (i.e. exits with an error code) it might be able to export it's errorstack, so we
 * can have an ez crashdump for ez debugging (and it would be kinda cool)
 *
 * We just have a few problems:
 * 1) When there is a chain of stack pushes (functions giving errors that cause other functions to
 *    give errors), we eventually should reach a point where this error is handled. At this point we
 *    need to be able to figure out which stack entries can be popped of
 * 2) How are we going to give every thread their own stack? (Proposed solution: hook into the thread
 *    creation kevent =0)
 */

#include <lightos/error.h>
#include <lightos/types.h>

typedef struct errstack_entry {
    /* Error number */
    error_t errno;
    /* The line this error was made from */
    u32 line;
    /* The function that pushed this entry */
    const char* function;
    /* Short description of the error */
    const char* desc;
    /* Pointer to the previous stack entry */
    struct errstack_entry* prev;
} errstack_entry_t;

error_t __push_lightos_err(error_t error, const char* function, u32 line, const char* desc);
error_t __pop_lightos_errstack(error_t error);

/*!
 * Called when a function returns an error
 */
#define push_lightos_err(errno, desc) \
    __push_lightos_err((errno), __func__, __LINE__, (desc))

/*!
 * Called when a caller might expect a function to
 * push an error
 *
 * func has to either be an error_t value, or a function call
 * that returns error_t
 */
#define expect_lightos_err(func) \
    __pop_lightos_errstack(func)

#endif // !__LIGHTOS_ERROR_STACK__

#ifndef __ANIVA_ERROR_WRAPPER__
#define __ANIVA_ERROR_WRAPPER__

#include <libk/stddef.h>
#include <logging/log.h>

/*
 * TODO: remake
 *
 * The error object is too big and clunky
 * We can do a few things:
 *  - reform to integer codes, like linux
 *  - use kevents for errors
 *  - combine
 * I had a few ideas to make errors in the kernel more streamlined. Most of them have to do with
 * a concise and system-wide constant way to handle errors, rather than using ErrorOrPtr here, ints there
 * and bools elsewhere. This is funky and makes for unpredictable kernel behaviour. Firstly, we need
 * to look at these things:
 *  - Must() calls must be replaced with ASSERT
 *  - kernel_panic must be reserved as a last resort
 *  - a form of dynamic, recoverable 'assertion' must be looked into
 *  - Generic error tracking must be emplaced
 *
 * When working with errors, we must remember that everything in the kernel stems from a single context. This context
 * can be setup in such a way to have consistant and precise tracking of errors, so that they can be recoverable as
 * often as possible
 */

/* This is the refined version of kernel error handling */
typedef int kerror_t;

#define KERR_FATAL_BIT (0x80000000UL)
/* No error */
#define KERR_NONE 0
/* Invalid arguments/results */
#define KERR_INVAL 1
/* I/O error */
#define KERR_IO 2
/* No memory */
#define KERR_NOMEM 3
/* Memory error */
#define KERR_MEM 4
/* No device */
#define KERR_NODEV 5
/* Device error */
#define KERR_DEV 6
/* No driver */
#define KERR_NODRV 7
/* Driver error */
#define KERR_DRV 8
/* No connection (to absolutely whatever) */
#define KERR_NOCON 9
/* Could not find what we where looking for */
#define KERR_NOT_FOUND 10
/* Size was divergent from what we expected */
#define KERR_SIZE_MISMATCH 11
/* Got a nullpointer */
#define KERR_NULL 12
/* Entry is a duplicate when this isn't allowed */
#define KERR_DUPLICATE 13
/* Caller has no permission to do the opperation */
#define KERR_NOPERM 14
/* Case has already been handled, no further action needed */
#define KERR_HANDLED 15
/* We've been waiting too long broooo */
#define KERR_TIMEOUT 16
/* Range check failed */
#define KERR_RANGE 17
/* Types mismatch */
#define KERR_TYPE_MISMATCH 18

#define KERR_OK(err) ((err) == KERR_NONE)
#define KERR_ERR(err) ((kerror_t)(err) < KERR_NONE)

/*
 * Single frame in an error chain
 */
typedef struct kerror_frame {
    kerror_t code;
    const char* ctx;

    struct kerror_frame* prev;
} kerror_frame_t;

void init_kerror();

kerror_t push_kerror(kerror_t err, const char* ctx);
kerror_t pop_kerror(kerror_frame_t* frame);

void drain_kerrors();

static inline bool kerror_is_fatal(kerror_t err)
{
    return (err & KERR_FATAL_BIT) == KERR_FATAL_BIT;
}

/* ***************************************
 *  Old libk error code
 * ***************************************
 */

typedef enum _ANIVA_STATUS {
    ANIVA_FAIL = 0,
    ANIVA_WARNING = 1,
    ANIVA_SUCCESS = 2,
    ANIVA_NOMEM,
    ANIVA_NODEV,
    ANIVA_NOCONN,
    ANIVA_FULL,
    ANIVA_EMPTY,
    ANIVA_BUSY,
    ANIVA_INVAL,
    // TODO: more types?
} ANIVA_STATUS;

#define EOP_ERROR 0
#define EOP_NOMEM 1
#define EOP_INVAL 2
#define EOP_BUSY 3
#define EOP_NODEV 4
#define EOP_NOCON 5
#define EOP_NULL 6

static inline bool status_is_ok(ANIVA_STATUS status)
{
    return (status == ANIVA_SUCCESS);
}

NORETURN void __kernel_panic();
NORETURN void kernel_panic(const char* panic_message);

#define ASSERT(condition) ((condition) ? (void)0 : kernel_panic("Assertion failed! (In: " __FILE__ ") TODO: stacktrace!"))
#define ASSERT_MSG(condition, msg)                          \
    do {                                                    \
        if (!(condition)) {                                 \
            print("Assertion failed (In: " __FILE__ "): "); \
            kernel_panic(msg);                              \
        }                                                   \
    } while (0)

#endif // !__ANIVA_ERROR_WRAPPER__

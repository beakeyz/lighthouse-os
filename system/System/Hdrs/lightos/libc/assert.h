#ifndef __LIGHTENV_ASSERT__
#define __LIGHTENV_ASSERT__

#include <lightos/error.h>
#include <stdlib.h>

/* Make sure there is no double assert definition */
#ifdef assert
#undef assert
#endif

#define assert(thing)          \
    do {                       \
        if (!(thing)) {        \
            exit(-EBADASSERT); \
        }                      \
    } while (0)

#endif // !__LIGHTENV_ASSERT__

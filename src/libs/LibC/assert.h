#ifndef __LIGHTENV_ASSERT__
#define __LIGHTENV_ASSERT__

#include <stdlib.h>

#ifndef assert

#define assert(thing)   \
    do {                \
        if (!(thing)) { \
            exit(-2);   \
        }               \
    } while (0)

#endif

#endif // !__LIGHTENV_ASSERT__

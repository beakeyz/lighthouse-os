#ifndef __ANIVA_LIGHTENV_STR__
#define __ANIVA_LIGHTENV_STR__
#include <LibDef/def.h>

typedef char* str_t;

static inline size_t strlen(str_t string) {
    size_t s = 0;
    while (string[s] != 0)
        s++;
    return s;
}

#endif

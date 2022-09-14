#ifndef __VIRTUAL_RANGE__
#define __VIRTUAL_RANGE__
#include <libc/stddef.h>

typedef struct virtual_range {
    uint64_t base;
    size_t size;
} virtual_range_t;

virtual_range_t* new_vrange (uint64_t base, size_t size);

bool vrange_contains (virtual_range_t*, uint64_t vaddr);

uint64_t end (virtual_range_t*);

#endif // !__VIRTUAL_RANGE__

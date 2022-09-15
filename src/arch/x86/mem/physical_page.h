#ifndef __PHYSICAL_PAGE__
#define __PHYSICAL_PAGE__

#include <libc/stddef.h>

typedef struct phys_page_entry {
    union {
        uint64_t phys_page;
    } allocated;
} phys_page_entry_t;
#endif // !__PHYSICAL_PAGE__


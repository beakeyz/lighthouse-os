#include "virtual_range.h"
#include "libc/stddef.h"

virtual_range_t* new_vrange (uint64_t base, size_t size) {
    return nullptr;
}

bool vrange_contains (virtual_range_t* vrange, uint64_t vaddr) {
    return (vaddr > vrange->base && vaddr < end(vrange));
}

uint64_t end (virtual_range_t* vrange) {
    return vrange->base + vrange->size;
}

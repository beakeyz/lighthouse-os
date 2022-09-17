#ifndef __PML__
#define __PML__
#include <libc/stddef.h>

typedef union pml {
    // this struct represents the uint64_t raw_bits, only its components
    // are structured for our sanity =)
    struct {
        uintptr_t present_bit:1;
        uintptr_t writable_bit:1;
        uintptr_t user_bit:1;
        uintptr_t writethrough_bit:1;
        uintptr_t nocache_bit:1;
        uintptr_t accessed_bit:1;
        uintptr_t _available1:1;
        uintptr_t size:1;
        uintptr_t global:1;
        uintptr_t cow_pending:1;
        uintptr_t _available2:2;
        uintptr_t page:28;
        uintptr_t reserved:12;
        uintptr_t _available3:11;
        uintptr_t nx:1;
    } structured_bits;
    uint64_t raw_bits;
} pml_t;

#endif // !__PML__

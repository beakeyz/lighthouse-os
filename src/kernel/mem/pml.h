#ifndef __PML__
#define __PML__
#include <libk/stddef.h>

// TODO: this kinda sucks lol
typedef union pml {
    // this struct represents the uint64_t raw_bits, only its components
    // are structured for our sanity =)
    struct {
        uintptr_t present_bit:1;
        uintptr_t writable_bit:1;
        uintptr_t user_bit:1;
        uintptr_t writethrough_bit:1;
        uintptr_t nocache_bit:1;
        uintptr_t accessed_bit:1; // also called: dirty bit
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
} __attribute__((packed)) pml_t;

inline void set_writable (pml_t* pml, bool writable) { pml->structured_bits.writable_bit = writable; }
inline void set_present (pml_t* pml, bool present) { pml->structured_bits.present_bit = present; }
inline void set_user (pml_t* pml, bool user) { pml->structured_bits.user_bit = user; }
inline void set_nx (pml_t* pml, bool nx) { pml->structured_bits.nx = nx; }
inline void set_nocache (pml_t* pml, bool nocache) { pml->structured_bits.nocache_bit = nocache; }

inline bool get_writable (pml_t* pml) { return pml->structured_bits.writable_bit; }
inline bool get_present (pml_t* pml) { return pml->structured_bits.present_bit; }
inline bool get_user (pml_t* pml) { return pml->structured_bits.user_bit; }
inline bool get_nx (pml_t* pml) { return pml->structured_bits.nx; }
inline bool get_nocache (pml_t* pml) { return pml->structured_bits.nocache_bit; }

extern pml_t create_pagetable (uintptr_t paddr, bool writable); 
#endif // !__PML__

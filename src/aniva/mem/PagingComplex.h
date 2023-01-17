#ifndef __complex__
#define __complex__
#include <libk/stddef.h>

typedef enum {
  PDE_PRESENT = (1 << 0),
  PDE_READ_WRITE = (1 << 1),
  PDE_USER = (1 << 2),
  PDE_WRITE_THROUGH = (1 << 3),
  PDE_NO_CACHE = (1 << 4),
  PDE_HUGE_PAGE = (1 << 7),
  PDE_GLOBAL = (1 << 8),
  PDE_NX = 0x8000000000000000ULL,
} PDE_FLAGS;

typedef enum {
  PTE_PRESENT = (1 << 0),
  PTE_READ_WRITE = (1 << 1),
  PTE_USER = (1 << 2),
  PTE_WRITE_THROUGH = (1 << 3),
  PTE_NO_CACHE = (1 << 4),
  PTE_PAT = (1 << 7),
  PTE_GLOBAL = (1 << 8),
  PTE_NX = 0x8000000000000000ULL,
} PTE_FLAGS;

// TODO: this kinda sucks lol
typedef union PagingComplex {
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
  } pde_bits;
  struct {
    uintptr_t present_bit:1;
    uintptr_t writable_bit:1;
    uintptr_t user_bit:1;
    uintptr_t writethrough_bit:1;
    uintptr_t nocache_bit:1;
    uintptr_t accessed_bit:1;
    uintptr_t dirty_bit:1;
    uintptr_t PAT:1;
    uintptr_t global:1;
    uintptr_t cow_pending:1;
    uintptr_t _available2:2;
    uintptr_t page:28;
    uintptr_t reserved:12;
    uintptr_t _available3:11;
    uintptr_t nx:1;
  } pte_bits;
  uint64_t raw_bits;
} __attribute__((packed)) PagingComplex_t;

// It does not really matter which sub-struct we take here, since it seems that all these bits are the same anyway across pde and pte
ALWAYS_INLINE void set_writable (PagingComplex_t* complex, bool writable) { complex->pde_bits.writable_bit = writable; }
ALWAYS_INLINE void set_present (PagingComplex_t* complex, bool present) { complex->pde_bits.present_bit = present; }
ALWAYS_INLINE void set_user (PagingComplex_t* complex, bool user) { complex->pde_bits.user_bit = user; }
ALWAYS_INLINE void set_nx (PagingComplex_t* complex, bool nx) { complex->pde_bits.nx = nx; }
ALWAYS_INLINE void set_nocache (PagingComplex_t* complex, bool nocache) { complex->pde_bits.nocache_bit = nocache; }

ALWAYS_INLINE bool get_writable (PagingComplex_t* complex) { return complex->pde_bits.writable_bit; }
ALWAYS_INLINE bool get_present (PagingComplex_t* complex) { return complex->pde_bits.present_bit; }
ALWAYS_INLINE bool get_user (PagingComplex_t* complex) { return complex->pde_bits.user_bit; }
ALWAYS_INLINE bool get_nx (PagingComplex_t* complex) { return complex->pde_bits.nx; }
ALWAYS_INLINE bool get_nocache (PagingComplex_t* complex) { return complex->pde_bits.nocache_bit; }

ALWAYS_INLINE void set_bit (PagingComplex_t* complex, uintptr_t bit, bool value) {
  if (value) {
    complex->raw_bits |= bit;
  } else {
    complex->raw_bits &= ~bit;
  }
}

extern PagingComplex_t create_pagetable (uintptr_t paddr, bool writable); 
#endif // !__complex__

#ifndef __entry__
#define __entry__
#include <libk/stddef.h>

#define PDE_PRESENT         0x0000000000000001ULL
#define PDE_WRITABLE        0x0000000000000002ULL
#define PDE_USER            0x0000000000000004ULL
#define PDE_WRITE_THROUGH   0x0000000000000008ULL
#define PDE_NO_CACHE        0x0000000000000010ULL 
#define PDE_ACCESSED        0x0000000000000020ULL
#define PDE_DIRTY           0x0000000000000040ULL
#define PDE_HUGE_PAGE       0x0000000000000080ULL
#define PDE_GLOBAL          0x0000000000000100ULL
#define PDE_PAT             0x0000000000001000ULL
#define PDE_NX              0x8000000000000000ULL

#define PTE_PRESENT         0x0000000000000001ULL
#define PTE_WRITABLE        0x0000000000000002ULL
#define PTE_USER            0x0000000000000004ULL
#define PTE_WRITE_THROUGH   0x0000000000000008ULL
#define PTE_NO_CACHE        0x0000000000000010ULL
#define PTE_ACCESSED        0x0000000000000020ULL
#define PTE_DIRTY           0x0000000000000040ULL
#define PTE_PAT             0x0000000000000080ULL
#define PTE_GLOBAL          0x0000000000000100ULL
#define PTE_NX              0x8000000000000000ULL

// FIXME: split into two
typedef union pml_entry {
  uint64_t raw_bits;
} __attribute__((packed)) pml_entry_t;

static ALWAYS_INLINE bool pml_entry_is_bit_set(pml_entry_t* entry, uintptr_t bit) 
{
  return (entry->raw_bits & bit) == bit;
}

static ALWAYS_INLINE void pml_entry_set_bit(pml_entry_t* entry, uintptr_t bit, bool value) 
{
  if (value) entry->raw_bits |= bit;
  else entry->raw_bits &= ~(bit);
}

void pml_entry_set_bit(pml_entry_t* entry, uintptr_t bit, bool value);

#endif // !__entry__

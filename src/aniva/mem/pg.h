#ifndef __entry__
#define __entry__
#include "dev/debug/serial.h"
#include "libk/string.h"
#include <libk/stddef.h>

typedef enum {
  PDE_PRESENT = (1 << 0),
  PDE_READ_WRITE = (1 << 1),
  PDE_USER = (1 << 2),
  PDE_WRITE_THROUGH = (1 << 3),
  PDE_NO_CACHE = (1 << 4),
  PDE_HUGE_PAGE = (1 << 7),
  PDE_GLOBAL = (1 << 8),
  // last bit of the 64 bit integer
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
  // last bit of the 64 bit integer
  PTE_NX = 0x8000000000000000ULL,
} PTE_FLAGS;

// FIXME: split into two
typedef union pml_entry {
  uint64_t raw_bits;
} __attribute__((packed)) pml_entry_t;

static ALWAYS_INLINE bool pml_entry_is_bit_set(pml_entry_t* entry, uintptr_t bit) {
  return (entry->raw_bits & bit) == bit;
}

static ALWAYS_INLINE void pml_entry_set_bit(pml_entry_t* entry, uintptr_t bit, bool value) {
  // TODO: verifiy that this bit can be set
  if (value)
    entry->raw_bits |= bit;
  else
   entry->raw_bits &= ~(bit);
}

ALWAYS_INLINE void set_page_bit(pml_entry_t* entry, uintptr_t bit, bool value) {
  if (value) {
    entry->raw_bits |= bit;
  } else {
    entry->raw_bits &= ~bit;
  }
}

extern pml_entry_t create_pagetable (uintptr_t paddr, bool writable); 
#endif // !__entry__

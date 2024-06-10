#ifndef __ANIVA_MEM_PAGEDIR__
#define __ANIVA_MEM_PAGEDIR__

#include "libk/stddef.h"
#include "mem/pg.h"

typedef struct {
    vaddr_t m_kernel_high;
    vaddr_t m_kernel_low;
    pml_entry_t* m_root;
    paddr_t m_phys_root;
} page_dir_t;

#endif // !__ANIVA_MEM_PAGEDIR__

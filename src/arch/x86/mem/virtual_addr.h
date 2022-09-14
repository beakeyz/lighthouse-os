#ifndef __VIRTUAL_ADDR__
#define __VIRTUAL_ADDR__
#include <libc/stddef.h>
#include "arch/x86/kmain.h"

bool is_page_aligned (uint64_t vaddr) { return (vaddr & PAGE_SIZE) == 0;  }

#endif // !__VIRTUAL_ADDR__


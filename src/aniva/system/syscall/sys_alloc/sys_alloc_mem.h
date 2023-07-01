#ifndef __ANIVA_SYS_ALLOC_MEM__
#define __ANIVA_SYS_ALLOC_MEM__

#include <libk/stddef.h>

uint32_t sys_alloc_pages(size_t size, uint32_t flags, void** buffer);

#endif // !__ANIVA_SYS_ALLOC_MEM__

#ifndef __KMAIN__
#define __KMAIN__

#include <libc/stddef.h>
#define PAGE_SIZE 4096
#define PAGE_ALIGN __attribute__((aligned(PAGE_SIZE)));
#define VIRTUAL_BASE 0xffffffff80000000

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;
extern uintptr_t _kernel_physical_end;

#endif // !__KMAIN__

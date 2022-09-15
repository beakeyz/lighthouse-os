#ifndef __KMAIN__
#define __KMAIN__

#include <libc/stddef.h>
#define PAGE_SIZE 0x200000
#define PAGE_SIZE_BYTES 0x200000l
#define PAGE_ALIGN __attribute__((aligned(PAGE_SIZE)));
#define VIRTUAL_BASE 0xffffffff80000000

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;
extern uintptr_t _kernel_physical_end;

extern uint64_t boot_pml4t[];
extern uint64_t boot_pdpt[];
extern uint64_t boot_pdt[];
extern uint64_t boot_pt[];
#endif // !__KMAIN__

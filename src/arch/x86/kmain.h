#ifndef __KMAIN__
#define __KMAIN__

#include <arch/x86/mem/pml.h>
#include <libc/stddef.h>
#define PAGE_SIZE 0x200000
#define PAGE_SIZE_BYTES 0x200000UL
#define PAGE_ALIGN __attribute__((aligned(PAGE_SIZE)));
#define VIRTUAL_BASE 0xffffffff80000000

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;
extern uintptr_t _kernel_physical_end;

extern pml_t boot_pml4t[512];
extern pml_t boot_pdpt[];
extern pml_t boot_pdt[];
#endif // !__KMAIN__

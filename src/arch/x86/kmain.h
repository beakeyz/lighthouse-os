#ifndef __KMAIN__
#define __KMAIN__

#include <arch/x86/mem/pml.h>
#include <libc/stddef.h>

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;
extern uintptr_t _kernel_physical_end;

extern pml_t boot_pdpt[];
extern pml_t boot_pdt[];

void _clear_junky_memory ();
#endif // !__KMAIN__

ENTRY(start)

SECTIONS {
    /* lower half kernel ftw =D */
    . = 1M;

    _kernel_start =.;
    _kern_virtual_offset = 0xffffffff80000000;

    .multiboot_header ALIGN(4K):
    {
        KEEP(*(.multiboot_header))
    }

    .pre_text :
    {
        *(.pre_text)
    }

    . += _kern_virtual_offset;
    .text ALIGN(4K):
    {
        *(.text*)
	}

    .rodata  ALIGN(4K):
    {
        start_ctors = .;
        *(.ctors)
        *(.init_array)
        end_ctors = .;
        *(.rodata*)
    }

    .data  ALIGN(4K):
    {
        *(.data*)
    }

    .bss  ALIGN(4K) :
    {
        
        *(.pagetables*)

        *(COMMON)
        *(.bss*)
    }
    
    _kernel_end = .;
    _kernel_physical_end = . - _kern_virtual_offset;
}

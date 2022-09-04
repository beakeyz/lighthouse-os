ENTRY(start)

SECTIONS {
    /* lower half kernel ftw =D */
    . = 1M;

    _kernel_start =.;
    _kern_virtual_offset = 0xffffffff80000000;

    .multiboot_header ALIGN(8) :
    {
        KEEP(*(.multiboot_header))
    }

    .pre_text :
    {
        *(.pre_text)
    }

    . += _kern_virtual_offset;
    .text ALIGN(4K):AT (ADDR (.text) - _kern_virtual_offset)
    {
        *(.text*)
	}

    .rodata  ALIGN(4K):AT (ADDR (.rodata) - _kern_virtual_offset)
    {
        start_ctors = .;
        *(.ctors)
        *(.init_array)
        end_ctors = .;
        *(.rodata*)
    }

    .data  ALIGN(4K):AT (ADDR (.data) - _kern_virtual_offset)
    {
        *(.data*)
    }

    .bss  ALIGN(4K):AT (ADDR (.bss) - _kern_virtual_offset)
    {
        *(COMMON)
        *(.bss*)
    }
    
    _kernel_end = .;
    _kernel_physical_end = . - _kern_virtual_offset;
}

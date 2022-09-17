ENTRY(start)

SECTIONS {
    /* lower half kernel ftw =D */
    . = 1M;

    _kernel_start =.;

    .multiboot_header ALIGN(8) :
    {
        KEEP(*(.multiboot_header))
    }

    .pre_text :
    {
        *(.pre_text)
    }

    .text ALIGN(4K):AT (ADDR (.text))
    {
        *(.text*)
	}

    .rodata  ALIGN(4K):AT (ADDR (.rodata) )
    {
        start_ctors = .;
        *(.ctors)
        *(.init_array)
        end_ctors = .;
        *(.rodata*)
    }

    .data  ALIGN(4K):AT (ADDR (.data) )
    {
        *(.data*)
    }

    .bss  ALIGN(4K):AT (ADDR (.bss) )
    {
        *(.pts*)
        *(COMMON)
        *(.bss*)

        . = ALIGN(4K);
        *(.heap)
    }
    
    _kernel_end = .;
    _kernel_physical_end = . ;
}

ENTRY(start)

SECTIONS {
    /* lower half kernel ftw =D */
    . = 1M;
    _kernel_start =.;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot_header)
        *(.pre_text)
        text_begin = .;
        *(.text)
	}

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        start_ctors = .;
        *(.ctors)
        *(.init_array)
        end_ctors = .;
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K) 
    {
        *(COMMON)
        *(.bss)

        *(.pts)
        *(.heap)
    }
    
    _kernel_end = .;
    _kernel_physical_end = . ;

    /DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}

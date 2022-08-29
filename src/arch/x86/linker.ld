ENTRY(start)

SECTIONS {
    /* lower half kernel ftw =D */
    . = 1M;

    .multiboot_header ALIGN(4K):
    {
        KEEP(*(.multiboot_header))
    }

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
        *(COMMON)
        *(.bss*)
    }}

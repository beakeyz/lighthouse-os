ENTRY(start)

SECTIONS {
    . = 0x10000;

    . = ALIGN(4K);
    .text :
    {
        *(.multiboot_header)
        *(.text .text.*)
    }

    . = ALIGN(4K);
    .rodata :
    {
        *(.rodata.*)
    }

    . = ALIGN(4K);
    .data :
    {
        *(.data .data.*)
    }

    . = ALIGN(4K);
    .bss :
    {
        *(COMMON)
        *(.bss .bss.*)
    }
}

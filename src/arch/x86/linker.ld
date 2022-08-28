ENTRY(start)

SECTIONS
{
    /*kernel_phys_offset = 0xffffffff80000000;*/
    /*addr_kernel_start = .;*/
    /*. = kernel_phys_offset + 0x100000;*/
    . = 1M;

    .multiboot_header ALIGN(4K):
    {
        /* Be sure that multiboot header is at the beginning */
        KEEP(*(.multiboot_header))
    }

    .text ALIGN(4K):
    {
        *(.text*)
		*(.init)
		*(.fini)
		*(.ctors)
		*(.dtors)
    }

    .rodata  ALIGN(4K):
    {
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
    }

    kernel_end = .;
}

ENTRY(start)

PHDRS
{
    elf_headers PT_LOAD FILEHDR PHDRS FLAGS(PF_R) ;
    boot PT_LOAD ;
    text PT_LOAD ;
    data PT_LOAD ;
    bss PT_LOAD ;
}

SECTIONS {
    /* lower half kernel ftw =D */
    . = 1M;

    .boot ALIGN (4K): AT ( ADDR (.boot) )
    {
        /* ensure that the multiboot header is at the beginning */
        KEEP(*(.multiboot_header))
    }:boot

    .text ALIGN (4K) : AT ( ADDR (.text) )
    {
      *(.text*)
    } :text

    .rodata ALIGN (4K) : AT ( ADDR (.rodata) ) 
    {
        *(.rodata*)
    } :data

    .data ALIGN (4K) : AT ( ADDR (.data) ) 
    {
        *(.data*)
    } :data
  
    /* NOLOAD ? */
    .bss ALIGN (4K) : AT ( ADDR (.bss) )
    {
        *(COMMON)
        *(.bss)
    } :bss
}

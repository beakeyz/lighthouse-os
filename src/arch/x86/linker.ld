ENTRY(start)

PHDRS
{
  text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
  rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
  data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
}

SECTIONS {
  /* lower half kernel ftw =D */
  . = 1M;
  _kernel_start =.;

  .multiboot_header : {
    *(.multiboot_header*)
  } :text

  .text : AT(ADDR(.text))
  {
    *(.pre_text)
    text_begin = .;
    *(.text)
  } :text

  .rodata ALIGN(4K) : AT(ADDR(.rodata))
  {
    start_ctors = .;
    *(.ctors)
    *(.init_array)
    end_ctors = .;
    *(.rodata)
  } :rodata

  .data : AT(ADDR(.data))
  {
    *(.data)
  } :data

  .bss : AT(ADDR(.bss)) 
  {
    *(COMMON)
    *(.bss)

    *(.pts)
    *(.heap)
  } :data
    
  _kernel_end = .;
  _kernel_physical_end = . ;

  /DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}

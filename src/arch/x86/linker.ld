ENTRY(start)

SECTIONS {
  /* lower half kernel ftw =D */
  . = 0x100000;
  _kernel_start =.;

  .multiboot_header : {
    *(.multiboot_header*)
  } 

  .pre_text : {
    *(.pre_text)
  }

  .text ALIGN(8) : AT(ADDR(.text) )
  {
    text_begin = .;
    *(.text)
  } 

  .rodata ALIGN(8) : AT(ADDR(.rodata) )
  {
    start_ctors = .;
    *(.ctors)
    *(.init_array)
    end_ctors = .;
    *(.rodata)
  }

  .data ALIGN(8) : AT(ADDR(.data) )
  {
    *(.data)
  }

  .bss ALIGN(8) : AT(ADDR(.bss) ) 
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

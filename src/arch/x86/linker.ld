ENTRY(start)

SECTIONS {
  /* lower half kernel ftw =D */
  . = 1M;
  _kernel_start =.;
  _kernel_virt_off = 0xffffffff80000000;

  .multiboot_header : {
    *(.multiboot_header*)
  } 

  .pre_text : {
    *(.pre_text)
  }

  . += _kernel_virt_off;
  .text ALIGN(4K) : AT(ADDR(.text) - _kernel_virt_off)
  {
    text_begin = .;
    *(.text)
  } 

  .rodata ALIGN(4K) : AT(ADDR(.rodata) - _kernel_virt_off)
  {
    start_ctors = .;
    *(.ctors)
    *(.init_array)
    end_ctors = .;
    *(.rodata)
  }

  .data ALIGN(4K) : AT(ADDR(.data) - _kernel_virt_off)
  {
    *(.data)
  }

  .bss ALIGN(4K) : AT(ADDR(.bss) - _kernel_virt_off) 
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

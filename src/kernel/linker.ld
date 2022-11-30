ENTRY(start)

PHDRS
{
  text PT_LOAD FLAGS(0x4 | 0x1) ;
  data PT_LOAD FLAGS(0x4 | 0x2) ;
  bss PT_LOAD FLAGS(0x4 | 0x2) ;
}

SECTIONS {
  . = 1M;
  _kernel_start =.;

  .text ALIGN(4K) : AT(ADDR(.text) )
  {
    *(.multiboot_header*)
    *(.pre_text*)
    text_begin = .;
    *(.text*)
  } :text 

  .rodata ALIGN(4K) : AT(ADDR(.rodata) )
  {
    start_ctors = .;
    *(.ctors*)
    *(.init_array)
    end_ctors = .;
    *(.rodata*)
  } :data

  .data ALIGN(4K) : AT(ADDR(.data) )
  {
    *(.data)
    _syms_start = .;
    *(.syms)
    _syms_end = .;
  } :data

  .bss ALIGN(4K) : AT(ADDR(.bss) ) 
  {
    *(.pts)
    _bss_start = .;
    *(COMMON)
    *(.bss)
    _bss_end = .;
    *(.stack*)

    _initial_heap_start = .;
    *(.heap)
    _initial_heap_end = .;
  } :bss
    
  _kernel_end = .;
  _kernel_physical_end = . ;

  /DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}

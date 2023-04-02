ENTRY(start)

PHDRS
{
  text PT_LOAD FLAGS(0x4 | 0x1) ;
  data PT_LOAD FLAGS(0x4 | 0x2) ;
  bss PT_LOAD FLAGS(0x4 | 0x2) ;
}

SECTIONS {
  . = 1M;
  _kernel_virt_base_offset = 0xFFFFffff80000000;

  .pre_text ALIGN(4K) : AT(ADDR(.pre_text))
  {
    *(.multiboot_header*)
    *(.pre_text*)
  } :text

  . += _kernel_virt_base_offset;

  _kernel_start =.;

  .text ALIGN(4K) : AT(ADDR(.text)  - _kernel_virt_base_offset)
  {
    _text_begin = .;
    *(.__init*)
    *(.text*)
    _text_end = .;  
  } :text 

  .rodata ALIGN(4K) : AT(ADDR(.rodata) - _kernel_virt_base_offset)
  {
    _start_ctors = .;
    *(.ctors*)
    *(.init_array)
    _end_ctors = .;

    _kernel_pcdrvs_start = .;
    *(.kpcdrvs*)
    _kernel_pcdrvs_end = .;

    *(.rodata*)

  } :data

  .data ALIGN(4K) : AT(ADDR(.data) - _kernel_virt_base_offset)
  {
    *(.data)
    _syms_start = .;
    *(.syms)
    _syms_end = .;
  } :data

  .bss ALIGN(4K) : AT(ADDR(.bss) - _kernel_virt_base_offset) 
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

  /DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}

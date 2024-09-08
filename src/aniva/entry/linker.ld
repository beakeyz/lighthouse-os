ENTRY(start)

PHDRS
{
  text PT_LOAD FLAGS(0x4 | 0x1) ;
  data PT_LOAD FLAGS(0x4 | 0x2) ;
  bss PT_LOAD FLAGS(0x4 | 0x2) ;
}

SECTIONS {
  . = 1M;

  _kernel_virt_base_offset = 0xFFFFFFFF80000000;

  . += _kernel_virt_base_offset;

  _kernel_start =.;

  .pre_text ALIGN(4K) : AT(ADDR(.pre_text) - _kernel_virt_base_offset)
  {
    *(.multiboot_header*)
    *(.pre_text*)
  } :text

  .text ALIGN(4K) : AT(ADDR(.text)  - _kernel_virt_base_offset)
  {
    _text_begin = .;
    *(.__init*)
    *(.text*)
    _text_end = .;  
  } :text 

  .rodata ALIGN(4K) : AT(ADDR(.rodata) - _kernel_virt_base_offset)
  {
    /* Any global constructors */
    _start_ctors = .;
    *(.ctors*)
    *(.init_array)
    _end_ctors = .;

    /* Precompiled core drivers that are shipped with the kernel at all times */
    _kernel_core_drvs_start = .;
    *(.core_drvs*)
    _kernel_core_drvs_end = .;

    /* Precompiled base-drivers that are built into the kernel go here */
    _kernel_pcdrvs_start = .;
    *(.kpcdrvs*)
    _kernel_pcdrvs_end = .;

    /* Internal khandle drivers get put here */
    _kernel_khndl_drvs_start = .;
    *(.khndl_drvs*)
    _kernel_khndl_drvs_end = .;

    /* The symbols of the kernel will be placed here in a large symboltable */
    _start_ksyms = .;
    *(.ksyms*)
    _end_ksyms = .;

    *(.rodata*)

  } :data

  .data ALIGN(4K) : AT(ADDR(.data) - _kernel_virt_base_offset)
  {
    *(.data)
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

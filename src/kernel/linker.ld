OUTPUT_FORMAT(elf64-x86-64)
ENTRY(start)

PHDRS
{
  elf_headers PT_LOAD FILEHDR PHDRS FLAGS(0x4) ;
  text PT_LOAD FLAGS(0x4 | 0x1) ;
  data PT_LOAD FLAGS(0x4 | 0x2) ;
  bss PT_LOAD FLAGS(0x4 | 0x2) ;
}

SECTIONS {
  . = 1M;
  _kernel_start =.;

  .elf_headers (SIZEOF_HEADERS) : AT (ADDR(.elf_headers) + SIZEOF_HEADERS)
  {
    start_of_elf_headers = .;
  } :elf_headers

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
    *(.data*)
  } :data

  .bss ALIGN(4K) : AT(ADDR(.bss) ) 
  {
    *(COMMON)
    *(.bss*)

    *(.pts*)
    *(.stack*)
    *(.heap*)
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

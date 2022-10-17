ENTRY(start)

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

PHDRS
{
  elf_headers PT_LOAD FILEHDR PHDRS FLAGS(PF_R) ;
  text PT_LOAD FLAGS(PF_R | PF_X) ;
  data PT_LOAD FLAGS(PF_R | PF_W) ;
  bss PT_LOAD FLAGS(PF_R | PF_W) ;
}

SECTIONS {
  /* lower half kernel ftw =D */
  . = 0x100000;
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

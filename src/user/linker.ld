/* TODO: get rid of this fucking thing lmao */
ENTRY(_start)

SECTIONS {

  /* Let's start user applications at 1Mib */
  . = 0x100000;
  
  .text ALIGN(4K) : AT(ADDR(.text))
  {
    _text_begin = .;
    *(.text*)
    _text_end = .;  
  }

  .rodata ALIGN(4K) : AT(ADDR(.rodata))
  {
    *(.rodata*)
  }

  .data ALIGN(4K) : AT(ADDR(.data))
  {
    *(.data)
  }

  .bss ALIGN(4K) : AT(ADDR(.bss)) 
  {
    _bss_start = .;
    *(COMMON)
    *(.bss)
    _bss_end = .;
  }

  /DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}

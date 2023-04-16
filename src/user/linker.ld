ENTRY(__lightapp_startup)

SECTIONS {
  . = 1M;
  
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

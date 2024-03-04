ENTRY(_start)

SECTIONS
{
    phys = .;

	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.text)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}

	.eh_frame BLOCK(4K) : ALIGN(4K) {
		*(.eh_frame)
	}

	end = .;

	/DISCARD/ :
	{
		*(.comment)
		*(.note.gnu.build-id)
	}
}

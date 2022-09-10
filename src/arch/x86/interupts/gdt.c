#include "gdt.h"
#include "arch/x86/dev/debug/serial.h"
#include <libc/string.h>
#include <libc/stddef.h>

static gdt_struct_t _gdt[1] = {{
    {
		{0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00},
		{0xFFFF, 0x0000, 0x00, 0x9A, (1 << 5) | (1 << 7) | 0x0F, 0x00},
		{0xFFFF, 0x0000, 0x00, 0x92, (1 << 5) | (1 << 7) | 0x0F, 0x00},
		{0xFFFF, 0x0000, 0x00, 0xFA, (1 << 5) | (1 << 7) | 0x0F, 0x00},
		{0xFFFF, 0x0000, 0x00, 0xF2, (1 << 5) | (1 << 7) | 0x0F, 0x00},
		{0x0067, 0x0000, 0x00, 0xE9, 0x00, 0x00},
	},
	{0x0000, 0x0000000000000000},
}};
// credit: toaruos (again)
void setup_gdt() {

    _gdt[0].ptr.limit = sizeof(_gdt[0].default_entries)-1;
	_gdt[0].ptr.base  = (uintptr_t)&_gdt[0].default_entries;

    if (&_gdt[0] == nullptr) {
        println("yikes");
    }

    load_gdt(&_gdt[0].ptr);
}


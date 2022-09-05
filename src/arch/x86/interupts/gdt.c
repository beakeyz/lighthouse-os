#include "gdt.h"
#include <libc/string.h>


gdt_struct_t gdt[32] = {{
    {
		{0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00},
		{0xFFFF, 0x0000, 0x00, 0x9A, (1 << 5) | (1 << 7) | 0x0F, 0x00},
		{0xFFFF, 0x0000, 0x00, 0x92, (1 << 5) | (1 << 7) | 0x0F, 0x00},
		{0xFFFF, 0x0000, 0x00, 0xFA, (1 << 5) | (1 << 7) | 0x0F, 0x00},
		{0xFFFF, 0x0000, 0x00, 0xF2, (1 << 5) | (1 << 7) | 0x0F, 0x00},
		{0x0067, 0x0000, 0x00, 0xE9, 0x00, 0x00},
	},
	{0x00000000, 0x00000000},
	{0x0000, 0x0000000000000000},
	{0,{0,0,0},0,{0,0,0,0,0,0,0},0,0,0},
}};

// credit: toaruos (again)
void setup_gdt() {
    for (int i = 1; i < 32; ++i) {
		memcpy(&gdt[i], &gdt[0], sizeof(*gdt));
	}

	for (int i = 0; i < 32; ++i) {
		gdt[i].ptr.limit = sizeof(gdt[i].default_entries)+sizeof(gdt[i].high_entry)-1;
		gdt[i].ptr.base  = (uintptr_t)&gdt[i].default_entries;

		uintptr_t addr = (uintptr_t)&gdt[i].tss;
		gdt[i].default_entries[5].limit_low = sizeof(gdt[i].tss);
		gdt[i].default_entries[5].base_low = (addr & 0xFFFF);
		gdt[i].default_entries[5].base_middle = (addr >> 16) & 0xFF;
		gdt[i].default_entries[5].base_high = (addr >> 24) & 0xFF;
		gdt[i].high_entry.base_high = (addr >> 32) & 0xFFFFFFFF;
	}

	extern void * stack_top;
	gdt[0].tss.rsp[0] = (uintptr_t)&stack_top;

    load_gdt(&gdt[0].ptr); 
}

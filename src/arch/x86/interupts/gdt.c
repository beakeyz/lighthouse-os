#include "gdt.h"
#include "arch/x86/dev/debug/serial.h"
#include <libc/string.h>
#include <libc/stddef.h>

_gdt_struct_t gdt __attribute__((aligned(0x1000))) = { 
    { 0, 0, 0, 0x00, 0x00, 0 },
    { 0, 0, 0, 0x9a, 0xa0, 0 },
    { 0, 0, 0, 0x92, 0xa0, 0 },
    { 0, 0, 0, 0x9a, 0xa0, 0 },
    { 0, 0, 0, 0x92, 0xa0, 0 },
};

// credit: toaruos (again)
void setup_gdt() {

    gdt_pointer_t gdtr;
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uintptr_t)&gdt;

    load_gdt(&gdtr);

    println("GDT setup");
}

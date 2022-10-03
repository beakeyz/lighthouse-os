#include "gdt.h"
#include "arch/x86/dev/debug/serial.h"
#include <arch/x86/mem/kmem_manager.h>
#include <libk/string.h>
#include <libk/stddef.h>


static gdt_pointer_t gdtr;
_gdt_struct_t gdt __attribute__((used));

// credit: toaruos (again)
__attribute__((used)) void setup_gdt() {

    memset(&gdt, 0, sizeof(gdt));

    gdtr.limit = 7 * sizeof(uintptr_t);
    gdtr.base = (uintptr_t)&gdt;

    // null
    gdt.null.raw = 0;

    // kcode
    uint64_t kernel_code = 0;
    kernel_code |= 0b1011 << 8; //type of selector
    kernel_code |= 1 << 12; //not a system descriptor
    kernel_code |= 0 << 13; //DPL field = 0
    kernel_code |= 1 << 15; //present
    kernel_code |= 1 << 21; //long-mode segment
    gdt.kernel_code.raw = kernel_code << 32;

    // kdata
    uint64_t kernel_data = 0;
    kernel_data |= 0b0011 << 8; //type of selector
    kernel_data |= 1 << 12; //not a system descriptor
    kernel_data |= 0 << 13; //DPL field = 0
    kernel_data |= 1 << 15; //present
    kernel_data |= 1 << 21; //long-mode segment
    gdt.kernel_data.raw = kernel_data << 32;

    // ucode
    uint64_t user_code = kernel_code | (3 << 13);
    gdt.user_code.raw = user_code;

    // udata
    uint64_t user_data = kernel_data | (3 << 13);
    gdt.user_data.raw = user_data;

    gdt.tss_low.raw = 0;
    gdt.tss_high.raw = 0;

    //load the new gdt
    asm volatile("lgdt %0" :: "m"(gdtr));

    flush_gdt();

    println("GDT setup");
}

#include "gdt.h"
#include "arch/x86/dev/debug/serial.h"
#include <libc/string.h>
#include <libc/stddef.h>

#define KRNL_CODE 0x00AF9B000000FFFF  //0x08: kernel code
#define KRNL_DATA 0x00AF93000000FFFF  //0x10: kernel data
#define USR_CODE 0x00AFFB000000FFFF  //0x18: user code
#define USR_DATA 0x00AFF3000000FFFF  //0x20: user data

static gdt_entry_t _gdt[7] = {0};
static gdt_pointer_t default_ptr;

static inline void fill_table () {
    for (uintptr_t i = 0; i < 7; ++i) {
        switch (i) {
            case 1:
                _gdt[i].raw = KRNL_CODE;
                break;
            case 2:
                _gdt[i].raw = KRNL_DATA;
                break;
            case 3:
                _gdt[i].raw = USR_CODE;
                break;
            case 4:
                _gdt[i].raw = USR_DATA;
                break;
            default:
                _gdt[i].raw = 0;
                break;
        } 
    }
}

// credit: toaruos (again)
void setup_gdt() {

    fill_table();

    default_ptr.limit = sizeof(gdt_entry_t) * 7;
	default_ptr.base  = (uintptr_t)_gdt;

    // some inline assembly, cuz we love that =D
    asm volatile ("lgdt %0" :: "m"(default_ptr));

    // FIXME: huuuuge yoink from northport, but hey, its a gdt jump
    //
    //reset all selectors (except cs), then reload cs with a far return
    //NOTE: this is a huge hack, as we assume rip was pushed to the stack (call instruction was used to come here).
    asm volatile("\
        mov $0x10, %ax \n\
        mov %ax, %ds \n\
        mov %ax, %es \n\
        mov %ax, %fs \n\
        mov %ax, %gs \n\
        mov %ax, %ss \n\
        pop %rdi \n\
        push $0x8 \n\
        push %rdi \n\
        lretq \n\
    ");
}


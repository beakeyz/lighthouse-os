#include <arch/x86/dev/debug/serial.h>
#include <arch/x86/multiboot.h>
#include <libc/stddef.h>
#include <libc/string.h>
#include <arch/x86/interupts/gdt.h>

typedef void (*ctor_func_t)();

extern ctor_func_t start_ctors[];
extern ctor_func_t end_ctors[];

extern uint64_t _kernel_end;

static uintptr_t first_valid_addr = 0;

__attribute__((constructor)) void test () {

    println("[TESTCONSTRUCTOR] =D");
}

static void hang () {
    asm volatile ("hlt");
    __builtin_unreachable();
}

void _start (uint32_t mb_addr, uint32_t mb_magic) {

    init_serial();

    for (ctor_func_t* constructor = start_ctors; constructor < end_ctors; constructor++) {
        (*constructor)();
    }
    println("Hi from 64 bit land =D");

    // Verify magic number
    if (mb_magic == 0x36d76289) {
        println("multiboot passed test");
        first_valid_addr = mb_initialize((void*)mb_addr);
    } else {
        println("big yikes");
        hang();
    }
    
    uint32_t* t = (uint32_t*)0x666;

    uint32_t v = 0;
    memcpy(&v, &t, sizeof(v));

    if (v == 0x666) {
        println("hihi");
    }

    // since memory management is still very hard (;-;) I will do gdt and idt first, so
    // TODO: memmanager goes here \/

    // gdt
    setup_gdt();

    // idt


    // TODO: some things on the agenda:
    // 0. [ ] buff up libc ;-;
    // 1. [X] parse the multiboot header and get the data we need from the bootloader, like framebuffer, memmap, ect (when we have our own bootloader, we'll have to revisit this =\)
    // 2. [ ] setup the memory manager, so we are able to consistantly allocate pageframes, setup a heap and ultimately do all kinds of cool memory stuff
    // 3. [ ] load a brand new GDT and IDT in preperation for step 4
    // 4. [ ] setup interupts so exeptions can be handled and stuff (perhaps do this first so we can catch pmm initialization errors?)
    //      -   also keyboard and mouse handlers ect.
    // 5. [ ] setup devices so we can have some propper communitaction between the kernel and the hardware
    // 6. [ ] setup a propper filesystem (ext2 r sm, maybe even do this earlier so we can load files and crap)
    // 7. ???
    // 8. profit
    // 9. do more stuff but I dont wanna look this far ahead as of now -_-

}

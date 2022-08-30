#include <arch/x86/dev/debug/serial.h>
#include <arch/x86/multiboot.h>
#include <libc/stddef.h>
#include <libc/string.h>
// TODO test call?

typedef void (*ctor_func_t)();

extern ctor_func_t start_ctors[];
extern ctor_func_t end_ctors[];

uint32_t test_var = 0;

__attribute__((constructor)) void test () {
    test_var = 1;

    println("hi");
}

void _start (multiboot_info_t* info_ptr) {
    if (info_ptr->mods_count < 1)
        return;

    // Check for a memorymap
    char* d = "test";

    int i = strcmp(d, "test");
    if (i != 0) {
        return;
    }

    // start serial
    init_serial();

    // global constructors
    for (ctor_func_t* constructor = start_ctors; constructor < end_ctors; constructor++) {
        (*constructor)();
    }

    // TODO: Memory manager
    // deal with the memory map here

    // TODO: init gdt and idt
    
    // TODO: interupts (irqs and isrs)

    // TODO: init devices 

    // TODO: init fleSystem

    // TODO: pic timer 

    // TODO: ACPI and stuff

    // TODO: Scheduler and threads

    // TODO: start main thread for services

    if (test_var == 0){
        return;
    }

    for (;;) {}
}

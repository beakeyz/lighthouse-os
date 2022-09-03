#include <arch/x86/dev/debug/serial.h>
#include <arch/x86/multiboot.h>
#include <libc/stddef.h>
#include <libc/string.h>
// TODO test call?

typedef void (*ctor_func_t)();

extern ctor_func_t start_ctors[];
extern ctor_func_t end_ctors[];

extern uint64_t _kernel_end;

uint32_t test_var = 0;

__attribute__((constructor)) void test () {
    test_var = 1;

    println("[TESTCONSTRUCTOR] =D");
}

void _start () {

    for (ctor_func_t* constructor = start_ctors; constructor < end_ctors; constructor++) {
        (*constructor)();
    }

    init_serial();

    println("Hi from 64 bit land =D");


}

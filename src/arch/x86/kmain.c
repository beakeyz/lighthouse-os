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
}


void _start (multiboot_info_t* info_ptr) {
    if (info_ptr->mods_count < 1)
        return;

    // Check for a memorymap
    char* d = "test";

    int i = strcmp(d, "test1");
    if (i != 0) {
        return;
    }

    for (ctor_func_t* constructor = start_ctors; constructor < end_ctors; constructor++) {
        (*constructor)();
    }

    if (test_var == 0){
        return;
    }

    for (;;) {}
}

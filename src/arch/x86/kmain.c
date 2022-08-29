#include <arch/x86/multiboot.h>
#include <libc/stddef.h>
// TODO test call?

uint32_t test_var = 0;

__attribute__((constructor)) void test () {
    test_var = 1;
}


void _start (multiboot_info_t* info_ptr) {
    if (info_ptr->mods_count < 1)
        return;

    for (;;) {}
}

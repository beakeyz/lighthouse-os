#include "error.h"
#include "irq/interrupts.h"
#include <libk/string.h>
#include <mem/heap.h>

// x86 specific halt (duh)
NORETURN void __kernel_panic()
{
    // dirty system halt
    for (;;) {
        disable_interrupts();
        asm volatile("cld");
        asm volatile("hlt");
    }
}

static bool has_paniced = false;

// TODO: retrieve stack info and stacktrace, for debugging purposes
NORETURN void kernel_panic(const char* panic_message)
{
    disable_interrupts();

    if (has_paniced)
        goto skip_diagnostics;

    has_paniced = true;

    // kwarnf("[KERNEL PANIC] %s\n", panic_message);
    printf("[KERNEL PANIC] %s\n", panic_message);

    /* TMP: hack to generate ez stacktrace xD */
    // uint64_t* a = 0;
    //*a = 0;

skip_diagnostics:
    __kernel_panic();
}

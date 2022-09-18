#include "kmalloc.h"
#include "arch/x86/kmain.h"
#include <libc/string.h>
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>



void* kmalloc(size_t len) {
    
    // find the right entry for our length

    // grow the heap if necessary

    // insert the list in the entrystack

    ASSERT(len);

    if (len) {}
    return NULL;
}

void kfree (void* addr) {
    if (addr) {}
}

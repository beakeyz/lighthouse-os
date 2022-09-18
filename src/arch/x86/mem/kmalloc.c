#include "kmalloc.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmem_bitmap.h"
#include <libc/string.h>
#include <arch/x86/dev/debug/serial.h>


void init_kmalloc(kmem_bitmap_t* bitmap, uint8_t* heap_addr, size_t heap_size) {
    // TODO: works?
}

void* kmalloc(size_t len) {
    
    return NULL;
}

void* kfree (void* addr, size_t len) {
    return NULL;
}

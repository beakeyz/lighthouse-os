#include "arch/x86/kmain.h"
#include "libc/linkedlist.h"
#include <arch/x86/mem/kmalloc.h>
#include <arch/x86/mem/kmem_manager.h>
#include <libc/io.h>
#include <arch/x86/interupts/idt.h>
#include <arch/x86/interupts/interupts.h>
#include <arch/x86/dev/debug/serial.h>
#include <arch/x86/interupts/control/pic.h>
#include <arch/x86/multiboot.h>
#include <libc/stddef.h>
#include <libc/string.h>

typedef void (*ctor_func_t)();

extern ctor_func_t start_ctors[];
extern ctor_func_t end_ctors[];

static uintptr_t first_valid_addr = 0;
static uintptr_t first_valid_alloc_addr = (uintptr_t)&_kernel_end;

void* mb_ptr = NULL;

__attribute__((constructor)) void test () {

    println("[TESTCONSTRUCTOR] =D");
}

static void hang () {
    asm volatile ("hlt");
    __builtin_unreachable();
}

int thing (registers_t* regs) {
    if (regs) {}
    println("funnie");
    return 1;
}

void _clear_junky_memory () {
    struct multiboot_tag_mmap* mmap = get_mb2_tag(mb_ptr, 6);
    void* e = mmap->entries;
    println(to_string((uintptr_t)mb_ptr));
    while((uintptr_t)e < (uintptr_t)mmap + mmap->size) {
        println("hah");
        struct multiboot_mmap_entry* cur_entry = (void*)e;
        if (cur_entry->type == 1) {
            for (uintptr_t base = cur_entry->addr; base < cur_entry->addr + (cur_entry->len & 0xFFFFffffFFFFf000); base += 0x1000) {
                kmem_mark_frame_free(base);
            }
        }
        e += mmap->entry_size;
    }
}

void _start (uint32_t mb_addr, uint32_t mb_magic) {

    init_serial();

    for (ctor_func_t* constructor = start_ctors; constructor < end_ctors; constructor++) {
        (*constructor)();
    }
    println("Hi from 64 bit land =D");


    // Verify magic number
    if (mb_magic == 0x36d76289) {
        // parse multiboot
        mb_initialize((void*)mb_addr, &first_valid_addr, &first_valid_alloc_addr);
    } else {
        println("big yikes");
        hang();
    }
    // setup pmm

    init_kmem_manager(mb_addr, first_valid_addr, first_valid_alloc_addr);

    // gdt
    //setup_idt();
    //init_interupts();
    // FIXME: still crashing =(
    //enable_interupts();

    // FIXME: using kmem_alloc raw probably is not a great idea, so I'll have to finish 
    // kmalloc first, and then I'll continue testing here.
    list_t* list = kmem_alloc(SMALL_PAGE_SIZE);
    if (list) {
        println("resetting head and end");
        list->head = 0;
        list->end = 0;
        node_t* node = (node_t*)&list + sizeof(list_t) + 1;
        if (node) {
            println("initializing node");
            node->next = 0;
            node->prev = 0;
            node->data = (void*)69420;
            println("adding node");
            list->head = node;
            list->end = node;
            // if the memory allocation works, we see the funnie number 
            // in the debug console!
            println(to_string((uintptr_t)node->data));
        }
    }

    // TODO: some thins on the agenda:
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

    for(;;) {
        asm("hlt");
    }
}

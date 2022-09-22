#include "arch/x86/kmain.h"
#include "arch/x86/interupts/gdt.h"
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
    init_kheap();

    // gdt
    setup_gdt();
    setup_idt();
    init_interupts();
    // FIXME: still crashing =(
    //enable_interupts();

    // FIXME: using kmem_alloc raw probably is not a great idea, so I'll have to finish 
    // kmalloc first, and then I'll continue testing here.

    // freed
    void* mock = kmalloc(SMALL_PAGE_SIZE);
    ASSERT(mock);
    
    // freedd
    list_t* dummy_list = kmalloc(sizeof(list_t));
    ASSERT(dummy_list);

    quick_print_node_sizes();

    // 0
    void* mock_2 = kmalloc(SMALL_PAGE_SIZE);
    ASSERT(mock_2);

    quick_print_node_sizes();

    // 1
    node_t* dummy_node = kmalloc(sizeof(node_t));
    ASSERT(dummy_node);
    void* _1 = kmalloc(SMALL_PAGE_SIZE);
    void* _2 = kmalloc(SMALL_PAGE_SIZE);
    void* _3 = kmalloc(SMALL_PAGE_SIZE);
    ASSERT(_1);
    ASSERT(_2);
    ASSERT(_3);

    kfree(_2);
    quick_print_node_sizes();

    void* thing = kmalloc(sizeof(node_t));
    // this proves that dummy_node is placed in the free node BEFORE the mock_2 allocation
    // (which was freed earlier)
    // TODO: the linkedlist is broken due to this order of frees and mallocs: find a fix for this

    quick_print_node_sizes();

    kfree(thing);
    quick_print_node_sizes();

    // TODO: some thins on the agenda:
    // 0. [ ] buff up libc ;-;
    // 1. [X] parse the multiboot header and get the data we need from the bootloader, like framebuffer, memmap, ect (when we have our own bootloader, we'll have to revisit this =\)
    // 2. [X] setup the memory manager, so we are able to consistantly allocate pageframes, setup a heap and ultimately do all kinds of cool memory stuff
    // 3. [ ] load a brand new GDT and IDT in preperation for step 4
    // 4. [ ] setup interupts so exeptions can be handled and stuff (perhaps do this first so we can catch pmm initialization errors?)
    //      -   also keyboard and mouse handlers ect.
    // 5. [ ] setup devices so we can have some propper communitaction between the kernel and the hardware
    // 6. [ ] setup a propper filesystem (ext2 r sm, maybe even do this earlier so we can load files and crap)
    // 7. ???
    // 8. profit
    // 9. do more stuff but I dont wanna look this far ahead as of now -_-

    while (true) {
        asm("hlt");
    }
}

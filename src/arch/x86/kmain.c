#include "arch/x86/kmain.h"
#include "arch/x86/dev/framebuffer/framebuffer.h"
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
    asm volatile (
		".global _ret_from_preempt_source\n"
		"_ret_from_preempt_source:"
	);
    return 1;
}

void _start (struct multiboot_tag* mb_addr, uint32_t mb_magic) {

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

    struct multiboot_tag_framebuffer* fb = get_mb2_tag((uintptr_t*)mb_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
    struct multiboot_tag_framebuffer_common fb_common = fb->common;
    init_kmem_manager((uintptr_t)mb_addr, first_valid_addr, first_valid_alloc_addr);
    init_kheap();

    asm volatile (
		"mov $0x277, %%ecx\n" /* IA32_MSR_PAT */
		"rdmsr\n"
		"or $0x1000000, %%edx\n" /* set bit 56 */
		"and $0xf9ffffff, %%edx\n" /* unset bits 57, 58 */
		"wrmsr\n"
		: : : "ecx", "edx", "eax"
	);

    // gdt
    setup_gdt();
    setup_idt();


    // FIXME FIXME: WHYYYYYYYYYY
    // FIXME: still crashing =(
    //enable_interupts();

    // common kinda gets lost or something, so we'll save it =)
    fb->common = (struct multiboot_tag_framebuffer_common)fb_common;
    init_fb(fb);

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

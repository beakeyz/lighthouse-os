#include "arch/x86/kmain.h"
#include "arch/x86/dev/framebuffer/framebuffer.h"
#include "arch/x86/interupts/gdt.h"
#include "libk/linkedlist.h"
#include <arch/x86/dev/debug/serial.h>
#include <arch/x86/interupts/control/pic.h>
#include <arch/x86/interupts/idt.h>
#include <arch/x86/interupts/interupts.h>
#include <arch/x86/mem/kmalloc.h>
#include <arch/x86/mem/kmem_manager.h>
#include <arch/x86/multiboot.h>
#include <libk/io.h>
#include <libk/stddef.h>
#include <libk/string.h>

typedef void (*ctor_func_t)();

extern ctor_func_t start_ctors[];
extern ctor_func_t end_ctors[];

static uintptr_t first_valid_addr = 0;
static uintptr_t first_valid_alloc_addr = (uintptr_t)&_kernel_end;

__attribute__((constructor)) void test() { println("[TESTCONSTRUCTOR] =D"); }

static void hang() {
  while (true) {
    asm volatile("hlt");
  }
  __builtin_unreachable();
}

int thing(registers_t *regs) {
  uint8_t sc = in8(0x60);
  println("funnie");
  return 1;
}

void _start(struct multiboot_tag *mb_addr, uint32_t mb_magic) {

  init_serial();
  println(to_string((uint64_t)_start));

  for (ctor_func_t *constructor = start_ctors; constructor < end_ctors;
       constructor++) {
    (*constructor)();
  }
  println("Hi from 64 bit land =D");

  // Verify magic number
  if (mb_magic == 0x36d76289) {
    // parse multiboot
    mb_initialize((void *)mb_addr, &first_valid_addr, &first_valid_alloc_addr);
  } else {
    println("big yikes");
    hang();
  }

  struct multiboot_tag_framebuffer *fb =
      get_mb2_tag((uintptr_t *)mb_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
  struct multiboot_tag_framebuffer_common fb_common = fb->common;
  init_kmem_manager((uintptr_t)mb_addr, first_valid_addr,
                    first_valid_alloc_addr);
  init_kheap();


  setup_gdt();
  setup_idt();
  init_interupts();

  // NOTE: testhandler
  add_handler(1, thing);

  enable_interupts();

  // common kinda gets lost or something, so we'll save it =)
  fb->common = (struct multiboot_tag_framebuffer_common)fb_common;
  init_fb(fb);

  // TODO: some thins on the agenda:
  // 0. [ ] buff up libk ;-;
  // 1. [X] parse the multiboot header and get the data we need from the
  // bootloader, like framebuffer, memmap, ect (when we have our own bootloader,
  // we'll have to revisit this =\)
  // 2. [X] setup the memory manager, so we are able to consistantly allocate
  // pageframes, setup a heap and ultimately do all kinds of cool memory stuff
  // 3. [X] load a brand new GDT and IDT in preperation for step 4
  // 4. [X] setup interupts so exeptions can be handled and stuff (perhaps do
  // this first so we can catch pmm initialization errors?)
  //      -   also keyboard and mouse handlers ect.
  // 5. [ ] setup devices so we can have some propper communitaction between the
  // kernel and the hardware
  // 6. [ ] setup a propper filesystem (ext2 r sm, maybe even do this earlier so
  // we can load files and crap)
  // 7. ???
  // 8. profit
  // 9. do more stuff but I dont wanna look this far ahead as of now -_-

  while (true) {
    asm("hlt");
  }
}

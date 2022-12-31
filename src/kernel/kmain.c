#include <kernel/kmain.h>
#include "dev/disk/storage_controller.h"
#include "dev/pci/pci.h"
#include "interupts/control/interrupt_control.h"
#include "kernel/dev/framebuffer/framebuffer.h"
#include "kernel/interupts/gdt.h"
#include "libk/bitmap.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include "system/processor/processor.h"
#include <kernel/dev/debug/serial.h>
#include <kernel/interupts/control/pic.h>
#include <kernel/interupts/idt.h>
#include <kernel/interupts/interupts.h>
#include <kernel/mem/kmalloc.h>
#include <kernel/mem/kmem_manager.h>
#include <kernel/libk/multiboot.h>
#include <libk/io.h>
#include <libk/stddef.h>
#include <libk/string.h>

typedef void (*ctor_func_t)();

extern ctor_func_t _start_ctors[];
extern ctor_func_t _end_ctors[];

static uintptr_t first_valid_addr = 0;
static uintptr_t first_valid_alloc_addr = (uintptr_t)&_kernel_end;

GlobalSystemInfo_t g_GlobalSystemInfo;

__attribute__((constructor)) void test() { println("[TESTCONSTRUCTOR] =D"); }

registers_t* thing(registers_t *regs) {
  in8(0x60);
  println("funnie");
  return regs;
}

void _start(struct multiboot_tag *mb_addr, uint32_t mb_magic) {

  init_serial();
  println("Hi from 64 bit land =D");
  
  // Verify magic number
  if (mb_magic != 0x36d76289) {
    println("big yikes");
    kernel_panic("Can't verify multiboot header: invalid magic number");
  }

  // parse multiboot
  mb_initialize((void *)mb_addr, &first_valid_addr, &first_valid_alloc_addr);
  g_GlobalSystemInfo.m_total_multiboot_size = get_total_mb2_size((void*)mb_addr);

  // init bootstrap processor
  g_GlobalSystemInfo.m_bsp_processor = init_processor(0);

  init_kheap(); // FIXME: this heap impl is very bad. improve it

  init_kmem_manager((uintptr_t*)mb_addr, first_valid_addr, first_valid_alloc_addr);

  g_GlobalSystemInfo.m_multiboot_addr = (uintptr_t)kmem_kernel_alloc((uintptr_t)mb_addr, g_GlobalSystemInfo.m_total_multiboot_size + 8, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
  struct multiboot_tag_framebuffer *fb = get_mb2_tag((uintptr_t *)mb_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);

  g_GlobalSystemInfo.m_bsp_processor.fLateInit(&g_GlobalSystemInfo.m_bsp_processor);

  // NOTE: testhandler
  InterruptHandler_t* test = init_interrupt_handler(1, I8259, thing);
  add_handler(test);

  enable_interupts();

  init_fb(fb);
  // Next TODO: create a kernel pre-init tty

  init_acpi();

  init_pci();

  // TODO: ATA/NVMe/IDE support?
  init_storage_controller();

  // TODO: some thins on the agenda:
  // 0. [X] buff up libk ;-;
  // 1. [X] parse the multiboot header and get the data we need from the
  // bootloader, like framebuffer, memmap, ect (when we have our own bootloader,
  // we'll have to revisit this =\)
  // 2. [X] setup the memory manager, so we are able to consistantly allocate
  // pageframes, setup a heap and ultimately do all kinds of cool memory stuff
  // 3. [X] load a brand new GDT and IDT in preperation for step 4
  // 4. [X] setup interupts so exeptions can be handled and stuff (perhaps do
  // this first so we can catch pmm initialization errors?)
  //      -   also keyboard and mouse handlers ect.
  // 5. [X] setup devices so we can have some propper communitaction between the
  // kernel and the hardware, like pci and usb
  // 6. [ ] setup a propper filesystem (ext2 r sm, maybe even do this earlier so
  // we can load files and crap)
  // 7. ???
  // 8. profit
  // 9. do more stuff but I dont wanna look this far ahead as of now -_-

  for (;;) {
    asm volatile ("hlt");
  }
}

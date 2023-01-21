#include "error.h"
#include "dev/debug/serial.h"

// x86 specific halt (duh)
NORETURN void __kernel_panic() {
  // dirty system halt
  for (;;) {
    disable_heap_expantion();
    disable_interrupts();
    asm volatile ("cld");
    asm volatile ("hlt");
  }
}

// TODO: retrieve stack info and stacktrace, for debugging purposes
NORETURN void kernel_panic(const char* panic_message) {

  bool has_serial = true;
  bool has_framebuffer = false;

  if (has_serial) {
    print("[KERNEL PANIC] ");
    println(panic_message);
  } 

  if (has_framebuffer) {
    // prepare framebuffer error message
  }

  __kernel_panic();
}

#include "error.h"
#include "dev/debug/serial.h"
#include "interupts/interupts.h"
#include "dev/framebuffer/framebuffer.h"
#include <mem/heap.h>
#include <libk/string.h>

// x86 specific halt (duh)
NORETURN void __kernel_panic() {
  // dirty system halt
  for (;;) {
    disable_interrupts();
    asm volatile ("cld");
    asm volatile ("hlt");
  }
}

// TODO: retrieve stack info and stacktrace, for debugging purposes
NORETURN void kernel_panic(const char* panic_message) {

  disable_interrupts();

  bool has_serial = true;
  bool has_framebuffer = true;

  if (has_serial) {
    print("[KERNEL PANIC] ");
    println(panic_message);
  } 

  if (has_framebuffer) {
    /*
    for (int i = 0; i < strlen(panic_message); i++) {
      draw_char(i * 8, 0, panic_message[i]);
    } 
    */
    // prepare framebuffer error message
  }

  __kernel_panic();
}

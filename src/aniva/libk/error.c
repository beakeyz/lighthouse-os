#include "error.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/kterm/kterm.h"
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
  bool has_framebuffer = false;

  if (has_serial) {
    print("[KERNEL PANIC] ");
    println(panic_message);
  } 

  /*
  if (has_framebuffer) {

    const char* string = panic_message;

    packet_response_t* response = await(driver_send_packet("graphics.kterm", KTERM_DRV_DRAW_STRING, &string, strlen(string)));

  }
  */

  disable_interrupts();
  __kernel_panic();
}

#include "error.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "intr/interrupts.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
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

static bool has_paniced = false;

// TODO: retrieve stack info and stacktrace, for debugging purposes
NORETURN void kernel_panic(const char* panic_message) {

  disable_interrupts();

  if (has_paniced) 
    goto skip_diagnostics;

  has_paniced = true;

  log("[KERNEL PANIC] ");
  logln(panic_message);

skip_diagnostics:
  __kernel_panic();
}

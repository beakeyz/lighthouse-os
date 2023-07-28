#include "error.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/kterm/kterm.h"
#include "interrupts/interrupts.h"
#include "dev/framebuffer/framebuffer.h"
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

  bool has_serial = true;
  bool has_framebuffer = true;
  proc_t* current_proc;
  thread_t* current_thread;

  if (has_serial) {
    print("[KERNEL PANIC] ");
    println(panic_message);
  }

  current_proc = get_current_proc();
  current_thread = get_current_scheduling_thread();

  /* Let's not try to write to the kterm when we don't have the mapping... */
  if (!current_proc || !current_thread || current_proc->m_root_pd.m_root != kmem_get_krnl_dir())
    has_framebuffer = false;

  /* NOTE: crashes in userspace (duh) */
  if (has_framebuffer) {
    println_kterm("[KERNEL PANIC] ");
    println_kterm(panic_message);
  }
skip_diagnostics:
  __kernel_panic();
}

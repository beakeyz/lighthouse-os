#include "error.h"
#include "irq/faults/faults.h"
#include "irq/interrupts.h"
#include "logging/log.h"
#include <proc/thread.h>
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

static NAKED void generate_raw_traceback()
{
  asm volatile (
    "pushq %%r15                            \n"
    "pushq %%r14                            \n"
    "pushq %%r13                            \n"
    "pushq %%r12                            \n"
    "pushq %%r11                            \n"
    "pushq %%r10                            \n"
    "pushq %%r9                             \n"
    "pushq %%r8                             \n"
    "pushq %%rax                            \n"
    "pushq %%rbx                            \n"
    "pushq %%rcx                            \n"
    "pushq %%rdx                            \n"
    "pushq %%rbp                            \n"
    "pushq %%rsi                            \n"
    "pushq %%rdi                            \n"

    "movq %%rsp, %%rdi                      \n"
    "call generate_traceback                \n"

    "popq %%rdi                             \n"
    "popq %%rsi                             \n"
    "popq %%rbp                             \n"
    "popq %%rdx                             \n"
    "popq %%rcx                             \n"
    "popq %%rbx                             \n"
    "popq %%rax                             \n"
    "popq %%r8                              \n"
    "popq %%r9                              \n"
    "popq %%r10                             \n"
    "popq %%r11                             \n"
    "popq %%r12                             \n"
    "popq %%r13                             \n"
    "popq %%r14                             \n"
    "popq %%r15                             \n"
    ::: "memory"
  );
}

// TODO: retrieve stack info and stacktrace, for debugging purposes
NORETURN void kernel_panic(const char* panic_message) {

  disable_interrupts();

  kwarnf("[KERNEL PANIC] %s\n", panic_message);

  if (has_paniced) 
    goto skip_diagnostics;

  has_paniced = true;

  generate_raw_traceback();

  /* TMP: hack to generate ez stacktrace xD */
  //uint64_t a = 0;
  //*(uint64_t*)(a) = 0;

skip_diagnostics:
  __kernel_panic();
}

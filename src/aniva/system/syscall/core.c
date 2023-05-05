#include "core.h"
#include "system/processor/processor.h"

extern void processor_enter_interruption(registers_t* registers, bool irq);
extern void processor_exit_interruption(registers_t* registers);

static struct syscall __static_syscalls[] = {
  [EXIT]        = { EXIT, nullptr },
  [OPEN]        = { OPEN, nullptr },
  [CLOSE]       = { CLOSE, nullptr },
  [GETTIME]     = { GETTIME, nullptr },
};

static size_t static_syscall_count = (sizeof(__static_syscalls) / (sizeof(*__static_syscalls)));

/* This stub mimics interrupt behaviour */
NAKED void sys_entry() {
  asm (
    "movq %%rsp, %%gs:%c[usr_stck_offset]   \n"
    "movq %%gs:%c[krnl_stck_offset], %%rsp  \n"
    // TODO: where can we enable interrupts again?
    // FIXME: we are pushing user cs and ss here, but our
    // userspace still isn't up and running. Should we use kernel values here?
    // FIXME: we are pushing r11 and rcx twice here, and we already know they
    // contain predetirmined values. What should we do with them?
    // FIXME: can we add some fancy logic here to allow syscalls from ring 0?
    "pushq $0x1b                            \n"
    "pushq %%gs:%c[usr_stck_offset]         \n"
    "pushq %%r11                            \n"
    "pushq $0x23                            \n"
    "pushq %%rcx                            \n"
    "pushq $0                               \n"
    "pushq $0                               \n"
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

    "cld                                    \n"
    "sti                                    \n"

    "movq %%rsp, %%rdi                      \n"
    "movq $0, %%rsi                         \n"
    "call processor_enter_interruption      \n"
    "movq %%rsp, %%rdi                      \n"
    "call sys_handler                       \n"

    "movq %%rsp, %%rdi                      \n"
    "call processor_exit_interruption       \n"

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
    "addq $16, %%rsp                        \n"
    "popq %%rcx                             \n"
    "addq $16, %%rsp                        \n"

    "cli                                    \n"
    // 0.0 ??
    "popq %%rsp                             \n"
    "sysretq                                \n"
    :
    :
    [krnl_stck_offset]"i"(get_kernel_stack_offset()),
    [usr_stck_offset]"i"(get_user_stack_offset())
    : "memory"
    );
}

void sys_handler(registers_t* regs) {

  Processor_t* current_processor = get_current_processor();
  thread_t *current_thread = get_current_scheduling_thread();

  kernel_panic("SUCCESS! recieved syscall!");

}

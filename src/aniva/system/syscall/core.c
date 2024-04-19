#include "core.h"
#include "lightos/syscall.h"
#include "libk/flow/error.h"
#include "system/processor/processor.h"
#include "system/processor/registers.h"

#include "sys_alloc/sys_alloc_mem.h"
#include "sys_rw/sys_rw.h"
#include "sys_exit/sys_exit.h"
#include "sys_open/sys_open.h"
#include "sys_drv/sys_drv.h"
#include "sys_dev/sys_dev.h"
#include "sys_open/sys_dir.h"
#include "system/syscall/sys_exec/sys_exec.h"
#include "sys_proc/sys_proc.h"

extern void processor_enter_interruption(registers_t* registers, bool irq);
extern void processor_exit_interruption(registers_t* registers);

/*
 * TODO: instead of having a weird-ass static list, we
 * could dynamically register syscalls by using linker sections
 */
static syscall_t __static_syscalls[] = {
  [SYSID_EXIT]              = { 0, SYSID_EXIT, (sys_fn_t)sys_exit_handler },
  [SYSID_GET_RUNTIME_CTX]   = { 0, SYSID_GET_RUNTIME_CTX, (sys_fn_t)sys_get_runtime_ctx, },
  [SYSID_CLOSE]             = { 0, SYSID_CLOSE , (sys_fn_t)sys_close, },
  [SYSID_READ]              = { 0, SYSID_READ, (sys_fn_t)sys_read, },
  [SYSID_WRITE]             = { 0, SYSID_WRITE, (sys_fn_t)sys_write },
  [SYSID_OPEN]              = { 0, SYSID_OPEN, (sys_fn_t)sys_open },
  [SYSID_OPEN_FILE]         = { 0, SYSID_OPEN_FILE, (sys_fn_t)sys_open_file },
  [SYSID_OPEN_PROC]         = { 0, SYSID_OPEN_PROC, (sys_fn_t)sys_open_proc },
  [SYSID_OPEN_DRIVER]       = { 0, SYSID_OPEN_DRIVER, (sys_fn_t)sys_open_driver },
  [SYSID_SEND_MSG]          = { 0, SYSID_SEND_MSG, (sys_fn_t)sys_send_message, },
  [SYSID_ALLOC_PAGE_RANGE]  = { 0, SYSID_ALLOC_PAGE_RANGE, (sys_fn_t)sys_alloc_page_range, },
  [SYSID_SYSEXEC]           = { 0, SYSID_SYSEXEC, (sys_fn_t)sys_exec, },
  [SYSID_GET_HNDL_TYPE]     = { 0, SYSID_GET_HNDL_TYPE, (sys_fn_t)sys_get_handle_type, },
  [SYSID_OPEN_PVAR]         = { 0, SYSID_OPEN_PVAR, (sys_fn_t)sys_open_pvar, },
  [SYSID_GET_PVAR_TYPE]     = { 0, SYSID_GET_PVAR_TYPE, (sys_fn_t)sys_get_pvar_type, },
  [SYSID_CREATE_PVAR]       = { 0, SYSID_CREATE_PVAR, (sys_fn_t)sys_create_pvar, },
  [SYSID_CREATE_DIR]        = { 0, SYSID_CREATE_DIR, (sys_fn_t)sys_create_dir, },
  [SYSID_DIR_READ]          = { 0, SYSID_DIR_READ, (sys_fn_t)sys_dir_read, },
  [SYSID_SEEK]              = { 0, SYSID_SEEK, (sys_fn_t)sys_seek },
  [SYSID_GET_PROCESSTIME]   = { 0, SYSID_GET_PROCESSTIME, (sys_fn_t)sys_get_process_time, },
  [SYSID_SLEEP]             = { 0, SYSID_SLEEP, (sys_fn_t)sys_sleep, },
  [SYSID_GET_FUNCADDR]      = { 0, SYSID_GET_FUNCADDR, (sys_fn_t)sys_get_funcaddr, },
  [SYSID_GET_DEVINFO]       = { 0, SYSID_GET_DEVINFO, (sys_fn_t)sys_get_devinfo, },
  [SYSID_DEV_ENABLE]        = { 0, SYSID_DEV_ENABLE, (sys_fn_t)sys_enable_device, },
  { NULL },
};

static const size_t __static_syscall_count = (sizeof(__static_syscalls) / (sizeof(*__static_syscalls)));

/* 
 * This stub mimics interrupt behaviour 
 * TODO: move this to pure asm to avoid compiler funzies
 */
NAKED NOINLINE void sys_entry() {
  asm (
    "movq %%rsp, %%gs:%c[usr_stck_offset]   \n"
    "movq %%gs:%c[krnl_stck_offset], %%rsp  \n"
    // TODO: where can we enable interrupts again?
    // FIXME: we are pushing user cs and ss here, but our
    // userspace still isn't up and running. Should we use kernel values here?
    // FIXME: we are pushing r11 and rcx twice here, and we already know they
    // contain predetirmined values. What should we do with them?
    // FIXME: can we add some fancy logic here to allow syscalls from ring 0?
    "pushq %[user_ss]                       \n"
    "pushq %%gs:%c[usr_stck_offset]         \n"
    "pushq %%r11                            \n"
    "pushq %[user_cs]                       \n"
    "pushq %%rcx                            \n"
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
    "addq $8, %%rsp                        \n"
    "popq %%rcx                             \n"
    "addq $16, %%rsp                        \n"

    "cli                                    \n"
    // 0.0 ??
    "popq %%rsp                             \n"
    "sysretq                                \n"
    :
    :
    [krnl_stck_offset]"i"(get_kernel_stack_offset()),
    [usr_stck_offset]"i"(get_user_stack_offset()),
    [user_ss]"g"(GDT_USER_DATA | 3),
    [user_cs]"g"(GDT_USER_CODE | 3)
    : "memory"
    );
}

typedef struct syscall_args {
  uint64_t arg0;
  uint64_t arg1;
  uint64_t arg2;
  uint64_t arg3;
  uint64_t arg4;
  enum SYSID syscall_id;
} syscall_args_t;

static int __syscall_parse_args(registers_t* regs, syscall_args_t* args)
{
  /* Caller should handle this */
  if (!regs)
    return -1;

  /* Here, our beautiful calling convention */
  *args = (syscall_args_t) {
    .arg0 = regs->rbx,
    .arg1 = regs->rdx,
    .arg2 = regs->rdi,
    .arg3 = regs->rsi,
    .arg4 = regs->r8,
    .syscall_id = (enum SYSID)regs->rax,
  };

  return 0;
}

static void __syscall_set_retval(registers_t* regs, uintptr_t ret)
{
  regs->rax = ret;
}

static bool __syscall_verify_sysid(enum SYSID id)
{
  if (id >= __static_syscall_count)
    return false;
  /* Let's hope uninitialised entries are zeroed =) */
  if (!__static_syscalls[id].m_handler)
    return false;

  return true;
}

void sys_handler(registers_t* regs) {

  uintptr_t result;
  syscall_t call;
  syscall_args_t args;

  ASSERT_MSG(regs, "Somehow we got no registers from the syscall entry!");

  /* Should not fail lmao */
  ASSERT(__syscall_parse_args(regs, &args) == 0);

  /* Verify that this syscall id is valid and has a valid handler */
  if (!__syscall_verify_sysid(args.syscall_id)) {
    /* TODO: murder the disbehaving process */
    printf("Tried to call %d\n", args.syscall_id);
    kernel_panic("Tried to call an invalid syscall!");
  }

  /* Execute the main syscall handler (All permissions) */
  call = __static_syscalls[args.syscall_id];

  /*
   * Syscalls are always called from the context of the process that 
   * invoked them, so we can get its process objects simply by using the
   * get_current_... functions (for threads, processes, cpus, ect.)
   */
  result = call.m_handler(args.arg0, args.arg1, args.arg2, args.arg3, args.arg4);

  /*
   * TODO: Execute any other handlers (These handlers may not alter the syscall information,
   * like result value or registers, but are purely based in the kernel) 
   */

  __syscall_set_retval(regs, result);
}

#include "core.h"
#include "libk/flow/error.h"
#include "lightos/syscall.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "system/processor/processor.h"
#include "system/processor/registers.h"

extern void processor_enter_interruption(registers_t* registers, bool irq);
extern void processor_exit_interruption(registers_t* registers);

static USED void sys_handler(registers_t* regs);

static const sys_fn_t __syscall_map[] = {
    [SYSID_EXIT] = (sys_fn_t)sys_exit,
    [SYSID_GET_EXITVEC] = (sys_fn_t)sys_get_exitvec,
    [SYSID_CLOSE] = (sys_fn_t)sys_close,
    [SYSID_READ] = (sys_fn_t)sys_read,
    [SYSID_WRITE] = (sys_fn_t)sys_write,
    [SYSID_OPEN] = (sys_fn_t)sys_open,
    [SYSID_OPEN_IDX] = (sys_fn_t)sys_open_idx,
    [SYSID_OPEN_CONNECTED_IDX] = (sys_fn_t)sys_open_connected_idx,
    [SYSID_SEND_MSG] = (sys_fn_t)sys_send_msg,

    [SYSID_GET_OBJECT_TYPE] = (sys_fn_t)sys_get_object_type,
    [SYSID_SET_OBJECT_TYPE] = (sys_fn_t)sys_set_object_type,
    [SYSID_GET_OBJECT_KEY] = (sys_fn_t)sys_get_object_key,
    [SYSID_SET_OBJECT_KEY] = (sys_fn_t)sys_set_object_key,
    [SYSID_CONNECT_OBJECT] = (sys_fn_t)sys_connect_object,
    [SYSID_DISCONNECT_OBJECT] = (sys_fn_t)sys_disconnect_object,

    [SYSID_ALLOC_VMEM] = (sys_fn_t)sys_alloc_vmem,
    [SYSID_DEALLOC_VMEM] = (sys_fn_t)sys_dealloc_vmem,
    [SYSID_MAP_VMEM] = (sys_fn_t)sys_map_vmem,
    [SYSID_PROTECT_VMEM] = (sys_fn_t)sys_protect_vmem,

    [SYSID_SYSEXEC] = (sys_fn_t)sys_exec,
    [SYSID_OPEN_PROC_OBJ] = (sys_fn_t)sys_open_proc_obj,
    [SYSID_CREATE_PROC] = (sys_fn_t)sys_create_proc,
    [SYSID_DESTROY_PROC] = (sys_fn_t)sys_destroy_proc,

    [SYSID_GET_SYSVAR_TYPE] = (sys_fn_t)sys_get_sysvar_type,
    [SYSID_CREATE_SYSVAR] = (sys_fn_t)sys_create_sysvar,

    [SYSID_SEEK] = (sys_fn_t)sys_seek,
    [SYSID_GET_PROCESSTIME] = (sys_fn_t)sys_get_process_time,
    [SYSID_SLEEP] = (sys_fn_t)sys_sleep,
    [SYSID_GET_FUNCTION] = (sys_fn_t)sys_get_function,
};
static const size_t __syscall_map_sz = (sizeof(__syscall_map) / (sizeof(*__syscall_map)));

/*
 * This stub mimics interrupt behaviour
 * TODO: move this to pure asm to avoid compiler funzies
 */
NAKED NOINLINE void sys_entry()
{
    asm(
        "movq %%rsp, %%gs:%c[usr_stck_offset]   \n"
        "movq %%gs:%c[krnl_stck_offset], %%rsp  \n"
        // TODO: where can we enable interrupts again?
        // FIXME: we are pushing user cs and ss here, but our
        // userspace still isn't up and running. Should we use kernel values here?
        // FIXME: we are pushing r11 and rcx twice here, and we already know they
        // contain predetirmined values. What should we do with them?
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
        [krnl_stck_offset] "i"(get_kernel_stack_offset()),
        [usr_stck_offset] "i"(get_user_stack_offset()),
        [user_ss] "g"(GDT_USER_DATA | 3),
        [user_cs] "g"(GDT_USER_CODE | 3)
        : "memory");
}

typedef struct syscall_args {
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    enum SYSID syscall_id;
} syscall_args_t;

static inline int __syscall_parse_args(registers_t* regs, syscall_args_t* args)
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

static inline void __syscall_set_retval(registers_t* regs, uintptr_t ret)
{
    regs->rax = ret;
}

static inline bool __is_static_syscall(enum SYSID id)
{
    return id < __syscall_map_sz;
}

static inline bool __syscall_verify_sysid(enum SYSID id)
{
    if (!__is_static_syscall(id))
        return false;

    /* Let's hope uninitialised entries are zeroed =) */
    if (!__syscall_map[id])
        return false;

    return true;
}

/*!
 * @brief: The generic syscall handler of the kernel
 *
 * Currently only looks through the static syscall map to see if there is a matching function
 * pointer for the specified SYSID in the registers.
 *
 * TODO: Support custom syscall maps, Also TODO: check if that is a dumb idea
 */
static void sys_handler(registers_t* regs)
{
    uintptr_t result;
    syscall_t call;
    syscall_args_t args;
    thread_t* calling_thread;

    ASSERT_MSG(regs, "Somehow we got no registers from the syscall entry!");

    /* Should not fail lmao */
    ASSERT(__syscall_parse_args(regs, &args) == 0);

    /* Verify that this syscall id is valid and has a valid handler */
    if (!__syscall_verify_sysid(args.syscall_id)) {
        /* TODO: murder the disbehaving process */
        printf("Tried to call %d\n", args.syscall_id);
        kernel_panic("Tried to call an invalid syscall!");
    }

    calling_thread = get_current_scheduling_thread();

    ASSERT_MSG(calling_thread, "Syscall without a scheduling thread");

    /* We're syscalling */
    calling_thread->m_c_sysid = args.syscall_id;

    /* Execute the main syscall handler (All permissions) */
    call = (syscall_t) {
        .m_handler = __syscall_map[args.syscall_id],
        .m_id = args.syscall_id,
        .m_flags = SYSCALL_CALLED,
    };

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

    /*
     * If the sysid is invalid at this point, consider the thread
     * invalid and yield
     */
    if (!SYSID_IS_VALID(calling_thread->m_c_sysid))
        thread_stop(calling_thread);

    /* Invalidate the sysid (i.e. mark the end of this syscall) */
    SYSID_SET_VALID(calling_thread->m_c_sysid, false);
}

typedef struct dynamic_syscall {
    syscall_t* p_syscall;
    /*  */
} dynamic_syscall_t;

/*!
 * @brief: Add a dynamic syscall
 */
kerror_t install_syscall(uint32_t id, sys_fn_t handler)
{
    return 0;
}

kerror_t uninstall_syscall(uint32_t id)
{
    return 0;
}

/*!
 * @brief: Call a syscall from outside of a user process
 *
 * Idk why you would want to do this, but probably handy for debugging stuff
 */
uintptr_t call_syscall(uint32_t id, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{
    registers_t dummy_regs = { 0 };

    /* Prepare a dummy register structure */
    dummy_regs.rax = id;
    dummy_regs.rbx = arg0;
    dummy_regs.rdx = arg1;
    dummy_regs.rdi = arg2;
    dummy_regs.rsi = arg3;
    dummy_regs.r8 = arg4;

    sys_handler(&dummy_regs);

    /* sys_handler will put the syscall result in %rax */
    return dummy_regs.rax;
}

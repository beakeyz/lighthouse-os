#include "thread.h"
#include "dev/debug/serial.h"
#include "dev/kterm/kterm.h"
#include "dev/manifest.h"
#include "entry/entry.h"
#include "kevent/kevent.h"
#include "libk/data/queue.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include <mem/heap.h>
#include "mem/zalloc.h"
#include "proc/context.h"
#include "proc/ipc/tspckt.h"
#include "proc/kprocs/idle.h"
#include "proc/proc.h"
#include "libk/stack.h"
#include "sched/scheduler.h"
#include "socket.h"
#include <libk/string.h>
#include "core.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"
#include "system/resource.h"
#include "time/pit.h"
#include <mem/heap.h>

extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) __attribute__((used));
extern void thread_exit_init_state(thread_t *from, registers_t* regs) __attribute__((used));
/* Routine that ends our thread */
extern NORETURN void thread_end_lifecycle();

static __attribute__((naked)) void common_thread_entry(void) __attribute__((used));
static ALWAYS_INLINE void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t arg0, uintptr_t arg1);

static thread_t* __generic_idle_thread;

/*
 * Fills the userstack with certain data for userspace
 * and a trampoline so we can return from userspace when 
 * the process exits (aka returns)
 */
//static ErrorOrPtr __thread_populate_user_stack(thread_t* thread);

thread_t *create_thread(FuncPtr entry, ThreadEntryWrapper entry_wrapper, uintptr_t data, const char name[32], proc_t* proc, bool kthread) { // make this sucka
  thread_t *thread = kmalloc(sizeof(thread_t));

  if (!thread)
    return nullptr;

  memset(thread, 0, sizeof(thread_t));

  thread->m_self = thread;

  thread->m_lock = create_mutex(0);

  thread->m_cpu = get_current_processor()->m_cpu_num;
  thread->m_parent_proc = proc;
  thread->m_ticks_elapsed = 0;
  thread->m_max_ticks = DEFAULT_THREAD_MAX_TICKS;
  thread->m_has_been_scheduled = false;
  thread->m_tid = -1;

  thread->f_real_entry = (ThreadEntry)entry;
  thread->f_exit = (FuncPtr)thread_end_lifecycle;

  if (entry_wrapper) {
    thread->f_entry_wrapper = entry_wrapper;
  }

  memcpy(&thread->m_fpu_state, &standard_fpu_state, sizeof(FpuState));
  strcpy(thread->m_name, name);
  thread_set_state(thread, NO_CONTEXT);

  /* Allocate kernel memory for the stack */
  thread->m_kernel_stack_bottom = Must(__kmem_alloc_range(
        proc->m_root_pd.m_root,
        proc->m_resource_bundle,
        HIGH_MAP_BASE,
        DEFAULT_STACK_SIZE,
        KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_CREATE_USER,
        KMEM_FLAG_WRITABLE));

  /* Compute the kernel stack top */
  thread->m_kernel_stack_top = ALIGN_DOWN(thread->m_kernel_stack_bottom + DEFAULT_STACK_SIZE, 16);
  thread->m_user_stack_top = 0;
  thread->m_user_stack_bottom = 0;


  /* Zero memory, since we don't want random shit in our stack */
  memset((void *)thread->m_kernel_stack_bottom, 0x00, DEFAULT_STACK_SIZE);

  /* Do context before we assign the userstack */
  thread->m_context = setup_regs(kthread, proc->m_root_pd.m_root, thread->m_kernel_stack_top);

  /*
   * FIXME: right now, we try to remap the stack every time a thread is created,
   * this means that the stack of the previous thread (if there is one) of this
   * process gets corrupted, so we either need to give every thread its own stack,
   * or we try to somehow let every thread share one stack (which like how tf would that work lol)
   */
  if (!kthread) {
    thread->m_user_stack_bottom = HIGH_STACK_BASE;

    thread->m_user_stack_bottom = Must(__kmem_alloc_range(
        proc->m_root_pd.m_root,
        proc->m_resource_bundle,
        thread->m_user_stack_bottom, 
        DEFAULT_STACK_SIZE, 
        KMEM_CUSTOMFLAG_NO_REMAP | KMEM_CUSTOMFLAG_CREATE_USER,
        KMEM_FLAG_WRITABLE));

    /* TODO: subtract random offset */
    thread->m_user_stack_top = ALIGN_DOWN(thread->m_user_stack_bottom + DEFAULT_STACK_SIZE, 16);

    memset(
        (void*)Must(kmem_get_kernel_address(thread->m_user_stack_bottom, proc->m_root_pd.m_root))
        , 0
        , DEFAULT_STACK_SIZE);

    /* We don't touch rsp when the thread is not a kthread */
    thread->m_context.rsp = thread->m_user_stack_top;
  }

  /* Set the entrypoint last */
  thread_set_entrypoint(thread, (FuncPtr)thread->f_real_entry, data, 0);
  return thread;
}

thread_t *create_thread_for_proc(proc_t *proc, FuncPtr entry, uintptr_t args, const char name[32]) {
  if (proc == nullptr || entry == nullptr) {
    return nullptr;
  }

  const bool is_kernel = ((proc->m_flags & PROC_KERNEL) == PROC_KERNEL) ||
    ((proc->m_flags & PROC_DRIVER) == PROC_DRIVER);

  thread_t *t = create_thread(entry, NULL, args, name, proc, is_kernel);
  if (thread_prepare_context(t) == ANIVA_SUCCESS) {
    return t;
  }
  return nullptr;
}

thread_t *create_thread_as_socket(proc_t *proc, FuncPtr entry, uintptr_t arg0, FuncPtr exit_fn, SocketOnPacket on_packet_fn, char name[32], uint32_t* port) {

  uint32_t _port;

  if (!proc || !entry) {
    return nullptr;
  }

  _port = 0;

  if (port)
    _port = *port;

  threaded_socket_t *socket = create_threaded_socket(nullptr, exit_fn, on_packet_fn, _port, SOCKET_DEFAULT_SOCKET_BUFFER_SIZE);

  // nullptr should mean that no allocation has been done
  if (socket == nullptr) {
    return nullptr;
  }

  const bool is_kernel = ((proc->m_flags & PROC_KERNEL) == PROC_KERNEL) ||
    ((proc->m_flags & PROC_DRIVER) == PROC_DRIVER);

  thread_t *ret = create_thread(entry, nullptr, arg0, name, proc, is_kernel);

  // failed to create thread
  if (!ret) {
    goto dealloc_and_exit;
  }

  if (thread_prepare_context(ret) != ANIVA_SUCCESS) {
    goto dealloc_and_exit;
  }

  ret->m_socket = socket;
  socket->m_parent = ret;

  if (port)
    *port = socket->m_port;

  socket_enable(ret);

  return ret;

dealloc_and_exit:
  destroy_threaded_socket(socket);
  destroy_thread(ret);
  return nullptr;
}

void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t arg0, uintptr_t arg1) {
  contex_set_rip(&ptr->m_context, (uintptr_t)entry, arg0, arg1);
}

void thread_set_state(thread_t *thread, thread_state_t state) {
  // let's get a hold of the scheduler while doing this

  thread->m_current_state = state;
  // TODO: update thread context(?) on state change TODO: (??) onThreadStateChangeEvent?
}

// TODO: finish
// TODO: when this thread has gotten it's own heap, free that aswell
ANIVA_STATUS destroy_thread(thread_t *thread) {
  proc_t* parent_proc;

  if (!thread)
    return ANIVA_FAIL;

  parent_proc = thread->m_parent_proc;

  if (thread->m_socket) {
    destroy_threaded_socket(thread->m_socket);
  }

  destroy_mutex(0);

  Must(__kmem_dealloc(parent_proc->m_root_pd.m_root, parent_proc->m_resource_bundle, thread->m_kernel_stack_bottom, DEFAULT_STACK_SIZE));

  if (thread->m_user_stack_bottom) {
    Must(__kmem_dealloc(parent_proc->m_root_pd.m_root, parent_proc->m_resource_bundle, thread->m_user_stack_bottom, DEFAULT_STACK_SIZE));
  }

  kfree(thread);
  return ANIVA_SUCCESS;
}

thread_t* get_generic_idle_thread() {
  ASSERT_MSG(__generic_idle_thread, "Tried to get generic idle thread before it was initialized");
  return __generic_idle_thread;
}

/*
 * Routine to be called when a thread ends execution. We can just stick this to the 
 * end of the stack of the thread 
 * FIXME: when a user process ends this gets entered in usermode. That kinda stinks :clown:
 */
extern NORETURN void thread_end_lifecycle() {
  // TODO: report or cache the result somewhere until
  // It is approved by the kernel

  kernel_panic("thread_end_lifecycle");

  thread_t *current_thread = get_current_scheduling_thread();

  ASSERT_MSG(current_thread, "Can't end a null thread!");

  thread_set_state(current_thread, DYING);

  // yield will enable interrupts again (I hope)
  scheduler_yield();

  kernel_panic("thread_end_lifecycle returned!");
}

NAKED void common_thread_entry() {
  asm (
    "popq %rdi \n" // our beautiful thread
    "popq %rsi \n" // ptr to its registers
    //"call thread_exit_init_state \n"
    "jmp asm_common_irq_exit \n"
    );
}

// TODO: redo?
extern void thread_enter_context(thread_t *to) {

  // FIXME: uncomment asap
  ASSERT_MSG(to->m_current_state == RUNNABLE, "thread we switch to is not RUNNABLE!");

  // FIXME: remove?
  processor_t *current_processor = get_current_processor();
  thread_t* previous_thread = get_previous_scheduled_thread();

  // TODO: make use of this
  //struct context_switch_event_hook hook = create_context_switch_event_hook(to);
  //call_event(CONTEXT_SWITCH_EVENT, &hook);

  // Only switch pagetables if we actually need to interchange between
  // them, otherwise thats just wasted tlb
  if (previous_thread->m_context.cr3 != to->m_context.cr3) {
    if (to->m_context.cr3 == (uintptr_t)kmem_get_krnl_dir()) {
      /* The exception is the kernel page dir, since it is stored as a physical address */
      kmem_load_page_dir(to->m_context.cr3, false);
    } else {
      kmem_load_page_dir(kmem_to_phys(nullptr, to->m_context.cr3), false);
    }
  }

  // NOTE: for correction purposes
  to->m_cpu = current_processor->m_cpu_num;

  store_fpu_state(&to->m_fpu_state);

  thread_set_state(to, RUNNING);
}

// called when a thread is created and enters the scheduler for the first time
ANIVA_STATUS thread_prepare_context(thread_t *thread) {

  thread_set_state(thread, RUNNABLE);
  uintptr_t rsp = thread->m_kernel_stack_top;

  /* 
   * Stitch the exit function at the end of the thread stack 
   * NOTE: we can do this since STACK_PUSH first decrements the
   *       stack pointer and then it places the value
   */
  *(uintptr_t*)rsp = (uintptr_t)thread->f_exit;

  if ((thread->m_context.cs & 3) != 0) {
    STACK_PUSH(rsp, uintptr_t, GDT_USER_DATA | 3);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rsp);
  } else {
    STACK_PUSH(rsp, uintptr_t, 0);
    STACK_PUSH(rsp, uintptr_t, thread->m_kernel_stack_top);
  }

  STACK_PUSH(rsp, uintptr_t, thread->m_context.rflags);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.cs);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.rip);

  // dummy irs_num and err_code
  STACK_PUSH(rsp, uintptr_t, 0);
  STACK_PUSH(rsp, uintptr_t, 0);

  STACK_PUSH(rsp, uintptr_t, thread->m_context.r15);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.r14);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.r13);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.r12);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.r11);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.r10);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.r9);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.r8);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.rax);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.rbx);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.rcx);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.rdx);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.rbp);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.rsi);
  STACK_PUSH(rsp, uintptr_t, thread->m_context.rdi);

  // ptr to registers_t struct above
  STACK_PUSH(rsp, uintptr_t, rsp + 8);

  thread->m_context.rip = (uintptr_t) &common_thread_entry;
  thread->m_context.rsp = rsp;
  thread->m_context.rsp0 = thread->m_kernel_stack_top;
  thread->m_context.cs = GDT_KERNEL_CODE;
  return ANIVA_SUCCESS;
}

// used to bootstrap the iret stub created in thread_prepare_context
// only on the first context switch
void bootstrap_thread_entries(thread_t* thread) {

  thread->m_has_been_scheduled = true;

  ASSERT(thread->m_current_state != NO_CONTEXT);
  ASSERT(get_current_scheduling_thread() == thread);
  thread_set_state(thread, RUNNING);

  tss_entry_t *tss_ptr = &get_current_processor()->m_tss;
  tss_ptr->iomap_base = sizeof(get_current_processor()->m_tss);
  tss_ptr->rsp0l = thread->m_context.rsp0 & 0xffffffff;
  tss_ptr->rsp0h = (thread->m_context.rsp0 >> 32) & 0xffffffff;

  asm volatile (
    "movq %[new_rsp], %%rsp \n"
    "pushq %[thread] \n"
    "pushq %[new_rip] \n"
    "movq 16(%%rsp), %%rdi \n"
    "movq $0, %%rsi \n"
    "call processor_enter_interruption \n"
    "retq \n"
    :
    "=d"(thread)
    :
    [new_rsp]"g"(thread->m_context.rsp),
    [new_rip]"g"(thread->m_context.rip),
    [thread]"d"(thread)
    : "memory"
  );
}

/*
 * To be called from a faulthandler directly out of the userpacket context
 * which means interrupts are off
 */
bool thread_try_revert_userpacket_context(registers_t* regs, thread_t* thread)
{
  return false;
}

void thread_try_prepare_userpacket(thread_t* to)
{
}

void thread_switch_context(thread_t* from, thread_t* to) {

  // FIXME: saving context is still wonky!!!
  // without saving the context it kinda works,
  // but it keeps running the function all over again.
  // since no state is saved, is just starts all over

  if (from && from->m_current_state == RUNNING) {
    thread_set_state(from, RUNNABLE);
  }

  ASSERT_MSG(get_previous_scheduled_thread() == from, "get_previous_scheduled_thread() is not the thread we came from!");

  tss_entry_t *tss_ptr = &get_current_processor()->m_tss;

  if (from)
    save_fpu_state(&from->m_fpu_state);

  asm volatile (
    THREAD_PUSH_REGS
    // save rsp
    "movq %%rsp, %[old_rsp] \n"
    // save rip
    "leaq 1f(%%rip), %%rbx \n"
    "movq %%rbx, %[old_rip] \n"
    // restore tss
    "movq %[base_stack_top], %%rbx \n"
    "movl %%ebx, %[tss_rsp0l] \n"
    "shrq $32, %%rbx \n"
    "movl %%ebx, %[tss_rsp0h] \n"

    "movq %[new_rsp], %%rsp \n"
    "movq %%rbp, %[old_rbp] \n"
    "pushq %[thread] \n"
    "pushq %[new_rip] \n"

    // prepare for switch
    "cld \n"
    "movq 8(%%rsp), %%rdi \n"
    "jmp thread_enter_context \n"
    // pick up where we left of
    "1: \n"
    "addq $8, %%rsp \n"
    THREAD_POP_REGS
    :
    [old_rbp]"=m"(from->m_context.rbp),
    [old_rip]"=m"(from->m_context.rip),
    [old_rsp]"=m"(from->m_context.rsp),
    [tss_rsp0l]"=m"(tss_ptr->rsp0l),
    [tss_rsp0h]"=m"(tss_ptr->rsp0h),
      "=d"(to)
    :
    [new_rsp]"g"(to->m_context.rsp),
    [new_rip]"g"(to->m_context.rip),
    [base_stack_top]"g"(to->m_context.rsp0),
    [thread]"d"(to)
    : "memory", "rbx"
  );
}

// TODO: this thing
extern void thread_exit_init_state(thread_t *from, registers_t* regs) {
}


void thread_set_current_driver(thread_t* t, dev_manifest_t* driver)
{
  mutex_lock(t->m_lock);
  t->m_current_driver = driver;
  mutex_unlock(t->m_lock);
}

dev_manifest_t* thread_get_current_driver(thread_t* t)
{
  return t->m_current_driver;
}

void thread_block(thread_t* thread) {

  //processor_increment_critical_depth(get_current_processor());

  // TODO: log certain things (i.e. how did we get blocked?)

  thread_set_state(thread, BLOCKED);

  // FIXME: we do allow blocking other threads here, we need to
  // check if that comes with extra complications
  if (thread == get_current_scheduling_thread()) {
    scheduler_yield();
  }

  //processor_decrement_critical_depth(get_current_processor());
  //kernel_panic("Unimplemented thread_block");
}

/*
 * TODO: unblocking means returning to the runnable threadpool
 */
void thread_unblock(thread_t* thread) {

  ASSERT_MSG(thread->m_current_state == BLOCKED, "Tried to unblock a non-blocking thread!");

  if (get_current_scheduling_thread() == thread) {
    thread_set_state(thread, RUNNING);
    return;
  }

  thread_set_state(thread, RUNNABLE);
}

/*
 * TODO:
 */
void thread_sleep(thread_t* thread) {
  kernel_panic("Unimplemented thread_sleep");
}

/*
 * TODO:
 */
void thread_wakeup(thread_t* thread) {
  kernel_panic("Unimplemented thread_wakeup");
}

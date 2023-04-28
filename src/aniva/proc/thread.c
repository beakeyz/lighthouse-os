#include "thread.h"
#include "dev/debug/serial.h"
#include "interupts/interupts.h"
#include "entry/entry.h"
#include "libk/error.h"
#include "mem/kmem_manager.h"
#include <mem/heap.h>
#include "mem/zalloc.h"
#include "proc/proc.h"
#include "libk/stack.h"
#include "sched/scheduler.h"
#include "socket.h"
#include <libk/string.h>
#include "core.h"
#include "sync/atomic_ptr.h"
#include "system/processor/processor.h"
#include <mem/heap.h>

extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) __attribute__((used));
extern void thread_exit_init_state(thread_t *from, registers_t* regs) __attribute__((used));

static __attribute__((naked)) void common_thread_entry(void) __attribute__((used));
static ALWAYS_INLINE void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t data);

thread_t *create_thread(FuncPtr entry, ThreadEntryWrapper entry_wrapper, uintptr_t data, char name[32], proc_t* proc, bool kthread) { // make this sucka
  thread_t *thread = kmalloc(sizeof(thread_t));
  thread->m_self = thread;

  thread->m_cpu = get_current_processor()->m_cpu_num;
  thread->m_parent_proc = proc;
  thread->m_ticks_elapsed = 0;
  thread->m_max_ticks = DEFAULT_THREAD_MAX_TICKS;
  thread->m_has_been_scheduled = false;
  thread->m_entry_wrapper = thread_entry_wrapper;

  if (entry_wrapper) {
    thread->m_entry_wrapper = entry_wrapper;
  }

  memcpy(&thread->m_fpu_state, &standard_fpu_state, sizeof(FpuState));
  strcpy(thread->m_name, name);
  thread_set_state(thread, NO_CONTEXT);

  uint32_t stack_mem_flags = KMEM_FLAG_WRITABLE | (kthread ? KMEM_FLAG_KERNEL : 0);

  /* TODO: We should probably allocate this somewhere in the processes memory range */
  uintptr_t stack_bottom = Must(kmem_kernel_alloc_range(DEFAULT_STACK_SIZE, KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_IDENTITY, stack_mem_flags));
  memset((void *)stack_bottom, 0x00, DEFAULT_STACK_SIZE);

  thread->m_stack_bottom = stack_bottom;
  thread->m_stack_top = ALIGN_DOWN(stack_bottom + DEFAULT_STACK_SIZE, 16);

  // TODO: move away from using the root page dir of the current processor
  thread->m_context = setup_regs(kthread, proc->m_root_pd, thread->m_stack_top);
  thread->m_real_entry = (ThreadEntry)entry;

  thread_set_entrypoint(thread, (FuncPtr) thread->m_entry_wrapper, data);
  return thread;
} // make this sucka

thread_t *create_thread_for_proc(proc_t *proc, FuncPtr entry, uintptr_t args, char name[32]) {
  if (proc == nullptr || entry == nullptr) {
    return nullptr;
  }

  const bool is_kernel = proc->m_flags & PROC_KERNEL;

  thread_t *t = create_thread(entry, NULL, args, name, proc, is_kernel);
  if (thread_prepare_context(t) == ANIVA_SUCCESS) {
    return t;
  }
  return nullptr;
}

thread_t *create_thread_as_socket(proc_t *proc, FuncPtr entry, uintptr_t arg0, FuncPtr exit_fn, SocketOnPacket on_packet_fn, char name[32], uint32_t port) {
  if (proc == nullptr || entry == nullptr) {
    return nullptr;
  }

  threaded_socket_t *socket = create_threaded_socket(nullptr, exit_fn, on_packet_fn, port, SOCKET_DEFAULT_SOCKET_BUFFER_SIZE);

  // nullptr should mean that no allocation has been done
  if (socket == nullptr) {
    return nullptr;
  }

  const bool is_kernel = proc->m_flags & PROC_KERNEL;

  thread_t *ret = create_thread(entry, default_socket_entry_wrapper, arg0, name, proc, is_kernel);

  // failed to create thread
  if (!ret) {
    destroy_threaded_socket(socket);
    destroy_thread(ret);
    return nullptr;
  }

  if (thread_prepare_context(ret) != ANIVA_SUCCESS) {
    destroy_threaded_socket(socket);
    destroy_thread(ret);
    return nullptr;
  }

  ret->m_socket = socket;
  socket->m_parent = ret;

  return ret;
}

void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t data) {
  contex_set_rip(&ptr->m_context, (uintptr_t)entry, data);

  // put the current thread ptr into the arguments of the thread entry
  ptr->m_context.rsi = (uintptr_t)ptr;
}

void thread_set_state(thread_t *thread, thread_state_t state) {
  // let's get a hold of the scheduler while doing this

  thread->m_current_state = state;
  // TODO: update thread context(?) on state change TODO: (??) onThreadStateChangeEvent?
}

// TODO: finish
// TODO: when this thread has gotten it's own heap, free that aswell
ANIVA_STATUS destroy_thread(thread_t *thread) {
  if (thread->m_socket) {
    destroy_threaded_socket(thread->m_socket);
  }
  kmem_kernel_dealloc(thread->m_stack_bottom, DEFAULT_STACK_SIZE);
  kfree(thread);
  return ANIVA_FAIL;
}

void thread_entry_wrapper(uintptr_t args, thread_t* thread) {

  // pre-entry

  // call the actual entrypoint of the thread
  int result = thread->m_real_entry(args);

  // TODO: report or cache the result somewhere until
  // It is approved by the kernel

  // TODO: cleanup and removal from the scheduler
  disable_interrupts();

  thread_t *current_thread = get_current_scheduling_thread();

  println("Set thing to dying");
  thread_set_state(current_thread, DYING);

  // yield will enable interrupts again
  scheduler_yield();
}

NAKED void common_thread_entry() {
  asm (
    "popq %rdi \n" // our beautiful thread
    "popq %rsi \n" // ptr to its registers
    // "call thread_exit_init_state \n"
    "jmp asm_common_irq_exit \n"
    );
}

// TODO: redo?
extern void thread_enter_context(thread_t *to) {

  // FIXME: uncomment asap
  ASSERT_MSG(to->m_current_state == RUNNABLE, "thread we switch to is not RUNNABLE!");

  // FIXME: remove?
  Processor_t *current_processor = get_current_processor();
  thread_t* previous_thread = get_previous_scheduled_thread();

  // TODO: make use of this
  //struct context_switch_event_hook hook = create_context_switch_event_hook(to);
  //call_event(CONTEXT_SWITCH_EVENT, &hook);

  // Only switch pagetables if we actually need to interchange between
  // them, otherwise thats just wasted buffers
  if (previous_thread->m_context.cr3 != to->m_context.cr3) {
    write_cr3(to->m_context.cr3);
  }

  // NOTE: for correction purposes
  to->m_cpu = current_processor->m_cpu_num;

  store_fpu_state(&to->m_fpu_state);

  thread_set_state(to, RUNNING);

}

// called when a thread is created and enters the scheduler for the first time
ANIVA_STATUS thread_prepare_context(thread_t *thread) {

  thread_set_state(thread, RUNNABLE);
  uintptr_t rsp = thread->m_stack_top;

  if ((thread->m_context.cs & 3) != 0) {
    STACK_PUSH(rsp, uintptr_t, GDT_USER_DATA | 3);
    STACK_PUSH(rsp, uintptr_t, thread->m_context.rsp);
  } else {
    STACK_PUSH(rsp, uintptr_t, 0);
    STACK_PUSH(rsp, uintptr_t, thread->m_stack_top);
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
  thread->m_context.rsp0 = thread->m_stack_top;
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
  tss_ptr->rsp0l = thread->m_stack_top & 0xffffffff;
  tss_ptr->rsp0h = thread->m_stack_top >> 32;

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
    "movq %[stack_top], %%rbx \n"
    "movl %%ebx, %[tss_rsp0l] \n"
    "shrq $32, %%rbx \n"
    "movl %%ebx, %[tss_rsp0h] \n"
    // emplace new rsp
    "movq %[new_rsp], %%rsp \n"
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
    [old_rip]"=m"(from->m_context.rip),
    [old_rsp]"=m"(from->m_context.rsp),
    [tss_rsp0l]"=m"(tss_ptr->rsp0l),
    [tss_rsp0h]"=m"(tss_ptr->rsp0h),
      "=d"(to)
    :
    [new_rsp]"g"(to->m_context.rsp),
    [new_rip]"g"(to->m_context.rip),
    [stack_top]"g"(to->m_stack_top),
    [thread]"d"(to)
    : "memory", "rbx"
  );
}

// TODO: this thing
extern void thread_exit_init_state(thread_t *from, registers_t* regs) {
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

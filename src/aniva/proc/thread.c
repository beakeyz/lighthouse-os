#include "thread.h"
#include "dev/debug/serial.h"
#include "kmain.h"
#include "mem/kmem_manager.h"
#include <mem/heap.h>
#include "proc/proc.h"
#include "libk/stack.h"
#include "socket.h"
#include <libk/string.h>
#include "core.h"
#include <mem/heap.h>

extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) __attribute__((used));
extern void thread_exit_init_state(thread_t *from, registers_t* regs) __attribute__((used));

static __attribute__((naked)) void common_thread_entry(void) __attribute__((used));
static ALWAYS_INLINE void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t data);

thread_t *create_thread(FuncPtr entry, uintptr_t data, char name[32], bool kthread) { // make this sucka
  thread_t *thread = kmalloc(sizeof(thread_t));
  thread->m_self = thread;
  thread->m_cpu = get_current_processor()->m_cpu_num;
  thread->m_parent_proc = nullptr;
  thread->m_ticks_elapsed = 0;
  thread->m_max_ticks = DEFAULT_THREAD_MAX_TICKS;
  thread->m_has_been_scheduled = false;

  memcpy(&thread->m_fpu_state, &standard_fpu_state, sizeof(FpuState));
  strcpy(thread->m_name, name);
  thread_set_state(thread, NO_CONTEXT);

  uint32_t stack_mem_flags = KMEM_FLAG_WRITABLE;

  if (kthread) {
    stack_mem_flags |= KMEM_FLAG_KERNEL;
  }

  uintptr_t stack_bottom = Must(kmem_kernel_alloc_range(DEFAULT_STACK_SIZE, KMEM_CUSTOMFLAG_GET_MAKE,stack_mem_flags));
  memset((void *)stack_bottom, 0x00, DEFAULT_STACK_SIZE);

  thread->m_stack_bottom = stack_bottom;
  thread->m_stack_top = ALIGN_DOWN(stack_bottom + DEFAULT_STACK_SIZE, 16);
  thread->m_context = setup_regs(kthread, get_current_processor()->m_page_dir, thread->m_stack_top);
  thread->m_real_entry = entry;


  thread_set_entrypoint(thread, (FuncPtr) thread_entry_wrapper, data);
  return thread;
} // make this sucka

thread_t *create_thread_for_proc(proc_t *proc, FuncPtr entry, uintptr_t args, char name[32]) {
  if (proc == nullptr || entry == nullptr) {
    return nullptr;
  }

  thread_t *t = create_thread(entry, args, name, (proc->m_id == 0));
  t->m_parent_proc = proc;
  if (thread_prepare_context(t) == ANIVA_SUCCESS) {
    return t;
  }
  return nullptr;
}

thread_t *create_thread_as_socket(proc_t *proc, FuncPtr entry, char name[32], uint32_t port) {

  threaded_socket_t *socket = create_threaded_socket(nullptr, port, SOCKET_DEFAULT_SOCKET_BUFFER_SIZE);

  // nullptr should mean that no allocation has been done
  if (socket == nullptr) {
    return nullptr;
  }

  thread_t *ret = create_thread_for_proc(proc, entry, (uintptr_t)socket->m_buffers, name);
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
  thread->m_current_state = state;
  // TODO: update thread context(?) on state change
  // TODO: (??) onThreadStateChangeEvent?
}

// TODO: more
ANIVA_STATUS clean_thread(thread_t *thread) {
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
  thread->m_real_entry(args);

  // TODO: cleanup and removal from the scheduler
  disable_interrupts();

  thread_t *current_thread = get_current_scheduling_thread();

  thread_set_state(current_thread, DYING);

  // yield will enable interrupts again
  scheduler_yield();
}

NAKED void common_thread_entry() {
  asm (
    "popq %rdi \n" // our beautiful thread
    "popq %rsi \n" // ptr to its registers
    "call thread_exit_init_state \n"
    "jmp asm_common_irq_exit \n"
    );
}

// TODO: redo?
extern void thread_enter_context(thread_t *to) {
  println("$thread_enter_context");
  println(to->m_name);

  // FIXME: uncomment asap
  ASSERT(to->m_current_state == RUNNABLE);

  // FIXME: remove?
  Processor_t *current_processor = get_current_processor();

  // TODO: make use of this
  //struct context_switch_event_hook hook = create_context_switch_event_hook(to);
  //call_event(CONTEXT_SWITCH_EVENT, &hook);

  // crashes =/
  //write_cr3(to->m_context.cr3);

  // NOTE: for correction purposes
  to->m_cpu = current_processor->m_cpu_num;

  store_fpu_state(&to->m_fpu_state);

  thread_set_state(to, RUNNING);

  println("done setting up context");
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

  print("(INIT) Loading context of: ");
  println(thread->m_name);

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

  print("Saving context of: ");
  println(from->m_name);
  print("Loading context of: ");
  println(to->m_name);

  if (from->m_current_state == RUNNING) {
    thread_set_state(from, RUNNABLE);
  }

  ASSERT_MSG(get_previous_scheduled_thread() == from, "get_previous_scheduled_thread() is not the thread we came from!");

  tss_entry_t *tss_ptr = &get_current_processor()->m_tss;

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

  println("Context switch! (thread_exit_init_state)");
  //println(to_string((uintptr_t)from));
  //println(to_string(regs->cs));
  //kernel_panic("reached thread_exit_init_state");
}


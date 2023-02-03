#include "thread.h"
#include "dev/debug/serial.h"
#include "kmain.h"
#include "mem/kmem_manager.h"
#include <mem/kmalloc.h>
#include "proc/proc.h"
#include "libk/stack.h"
#include "socket.h"
#include <libk/string.h>
#include "core.h"
#include "sched/scheduler.h"

// TODO: move somewhere central

static __attribute__((naked)) void common_thread_entry(void) __attribute__((used));
extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) __attribute__((used));
extern void thread_exit_init_state(thread_t *from, registers_t* regs) __attribute__((used));

thread_t *create_thread(FuncPtr entry, uintptr_t data, char name[32], bool kthread) { // make this sucka
  thread_t *thread = kmalloc(sizeof(thread_t));
  thread->m_self = thread;
  thread->m_cpu = get_current_processor()->m_cpu_num;
  thread->m_parent_proc = nullptr;
  thread->m_ticks_elapsed = 0;
  thread->m_max_ticks = DEFAULT_THREAD_MAX_TICKS;
  thread->m_has_been_scheduled = false;

  strcpy(thread->m_name, name);
  thread_set_state(thread, NO_CONTEXT);

  uintptr_t stack_bottom = Must(kmem_kernel_alloc_range(DEFAULT_STACK_SIZE, KMEM_CUSTOMFLAG_GET_MAKE,KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL));
  thread->m_stack_top = ALIGN_DOWN(stack_bottom + DEFAULT_STACK_SIZE, 16);
  memset((void *)(thread->m_stack_top - DEFAULT_STACK_SIZE), 0x00, DEFAULT_STACK_SIZE);

  memcpy(&thread->m_fpu_state, &standard_fpu_state, sizeof(FpuState));

  thread->m_context = setup_regs(kthread, get_current_processor()->m_page_dir, thread->m_stack_top);
  contex_set_rip(&thread->m_context, (uintptr_t) entry, data);

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
// funny wrapper
void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t data) {
  contex_set_rip(&ptr->m_context, (uintptr_t)entry, data);
}

void thread_set_state(thread_t *thread, thread_state_t state) {
  thread->m_current_state = state;
  // TODO: update thread context(?) on state change
  // TODO: (??) onThreadStateChangeEvent?
}

ANIVA_STATUS kill_thread(thread_t *thread) {
  return ANIVA_FAIL;
} // kill thread and prepare for context swap to kernel

ANIVA_STATUS kill_thread_if(thread_t *thread, bool condition) {
  return ANIVA_FAIL;
} // kill if condition is me

// TODO: more
ANIVA_STATUS clean_thread(thread_t *thread) {
  if (thread->m_socket) {
    destroy_threaded_socket(thread->m_socket);
  }
  kfree(thread);
  return ANIVA_FAIL;
}

void exit_thread() {

  pause_scheduler();

  thread_set_state(get_current_scheduling_thread(), DYING);

  resume_scheduler();

  for (;;) {
    //kernel_panic("TODO: exit thread");
  }
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

  // FIXME: tls via FS base? or LDT (linux does this r sm)
  //wrmsr(MSR_FS_BASE, (uintptr_t)to);

  // crashes =/
  //write_cr3(to->m_context.cr3);

  // NOTE: for correction purposes
  to->m_cpu = current_processor->m_cpu_num;

  store_fpu_state(&to->m_fpu_state);

  thread_set_state(to, RUNNING);

  println("done setting up context");

  //if (!to->m_has_been_scheduled) {
  //  bootstrap_thread_entries(to);
  //}
}

// called when a thread is created and enters the scheduler for the first time
ANIVA_STATUS thread_prepare_context(thread_t *thread) {

  thread_set_state(thread, RUNNABLE);
  uintptr_t rsp = thread->m_stack_top;

  STACK_PUSH(rsp, uintptr_t, 0);
  STACK_PUSH(rsp, uintptr_t, (uintptr_t)&exit_thread);

  STACK_PUSH(rsp, uintptr_t, 0);
  STACK_PUSH(rsp, uintptr_t, thread->m_stack_top);
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

void thread_save_context(thread_t* thread) {

}

void thread_switch_context(thread_t* from, thread_t* to) {

  // FIXME: saving context is still wonky!!!
  // without saving the context it kinda works,
  // but it keep running the function all over again.
  // since no state is saved, is just starts all over

  print("Saving context of: ");
  println(from->m_name);

  thread_set_state(from, RUNNABLE);

  print("Loading context of: ");
  println(to->m_name);

  //ASSERT(get_current_scheduling_thread() == from);

  tss_entry_t *tss_ptr = &get_current_processor()->m_tss;

  save_fpu_state(&from->m_fpu_state);

  asm volatile (
    "pushfq \n"
    "pushq %%r15 \n"
    "pushq %%r14 \n"
    "pushq %%r13 \n"
    "pushq %%r12 \n"
    "pushq %%r11 \n"
    "pushq %%r10 \n"
    "pushq %%r9 \n"
    "pushq %%r8 \n"
    "pushq %%rax \n"
    "pushq %%rbx \n"
    "pushq %%rcx \n"
    "pushq %%rdx \n"
    "pushq %%rbp \n"
    "pushq %%rsi \n"
    "pushq %%rdi \n"
    "movq %%rsp, %[old_rsp] \n"
    "leaq 1f(%%rip), %%rbx \n"
    "movq %%rbx, %[old_rip] \n"
    "movq %[stack_top], %%rbx \n"
    "movl %%ebx, %[tss_rsp0l] \n"
    "shrq $32, %%rbx \n"
    "movl %%ebx, %[tss_rsp0h] \n"
    "movq %[new_rsp], %%rsp \n"
    "pushq %[thread] \n"
    "pushq %[new_rip] \n"
    "cld \n"
    "movq 8(%%rsp), %%rdi \n"
    "jmp thread_enter_context \n"
    "1: \n"
    //"addq $8, %%rsp \n"
    "popq %%rdi \n"
    "popq %%rsi \n"
    "popq %%rbp \n"
    "popq %%rdx \n"
    "popq %%rcx \n"
    "popq %%rbx \n"
    "popq %%rax \n"
    "popq %%r8  \n"
    "popq %%r9  \n"
    "popq %%r10 \n"
    "popq %%r11 \n"
    "popq %%r12 \n"
    "popq %%r13 \n"
    "popq %%r14 \n"
    "popq %%r15 \n"
    "popfq \n"
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
  println(to_string((uintptr_t)from));
  println(to_string(regs->rflags));
  //kernel_panic("reached thread_exit_init_state");
}

__attribute__((naked)) void common_thread_entry() {
  asm (
    "popq %rdi \n"
    "popq %rsi \n"
    "call thread_exit_init_state \n"
    "jmp asm_common_irq_exit \n"
  );
}

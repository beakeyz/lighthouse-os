#include "thread.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/kmem_manager.h"
#include <mem/heap.h>
#include "proc/context.h"
#include "proc/proc.h"
#include "libk/stack.h"
#include "sched/scheduler.h"
#include <libk/string.h>
#include "core.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"
#include <mem/heap.h>

extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) USED;
extern void thread_exit_init_state(thread_t *from, registers_t* regs) USED;
static NAKED void common_thread_entry(void) USED;

static thread_t* __generic_idle_thread;

int init_thread_state_stack(thread_state_stack_t* stack, uint32_t capacity)
{
  if (!stack || !capacity)
    return -1;

  memset(stack, 0, sizeof(*stack));

  stack->stack_capacity = capacity;
  stack->current_state_idx = 0;
  stack->base_state = DYING;

  stack->stack = kmalloc(sizeof(THREAD_STATE_t) * capacity);

  /* Yikes */
  if (!stack->stack)
    return -1;

  memset(stack->stack, 0, sizeof(THREAD_STATE_t) * capacity);

  return 0;
}

void destroy_thread_state_stack(thread_state_stack_t* stack)
{
  kfree(stack->stack);
}

int thread_state_stack_push(thread_state_stack_t* stac, THREAD_STATE_t state)
{
  stac->current_state_idx++;

  if (stac->current_state_idx >= stac->stack_capacity)
    return -1;

  stac->stack[stac->current_state_idx] = state;
  return 0;
}

int thread_state_stack_pop(thread_state_stack_t* stac, THREAD_STATE_t* state)
{
  THREAD_STATE_t _state;
  /* If we try to pop on an empty stack, revert it to the base state */
  if (!stac->current_state_idx) {
    *state = stac->stack[0] = stac->base_state;
    return 0;
  }

  /* There's a state to pop, yeet it */
  _state = stac->stack[stac->current_state_idx--];

  /* Export the previous state */
  if (state)
    *state = _state;

  return 0;
}

int thread_state_stack_peek(thread_state_stack_t* stac, THREAD_STATE_t* state)
{
  if (!state)
    return -1;

  *state = stac->stack[stac->current_state_idx];
  return 0;
}

thread_t *create_thread(FuncPtr entry, uintptr_t data, const char* name, bool kthread)
{
  thread_t *thread;

  thread = kmalloc(sizeof(thread_t));

  if (!thread)
    return nullptr;

  memset(thread, 0, sizeof(thread_t));

  thread->m_lock = create_mutex(0);

  thread->m_cpu = get_current_processor()->m_cpu_num;
  thread->m_ticks_elapsed = 0;
  thread->m_max_ticks = DEFAULT_THREAD_MAX_TICKS;

  thread->param0 = data;
  thread->f_entry = (ThreadEntry)entry;
  thread->f_exit = (FuncPtr)0;
  thread->m_name = strdup(name);

  memcpy(&thread->m_runtime_state.fpu_state, &standard_fpu_state, sizeof(FpuState));

  /* Initialize stack state */
  init_thread_state_stack(&thread->state, 32);

  /* Set the first state */
  thread_set_state(thread, NO_CONTEXT);
  return thread;
}

thread_t *create_thread_for_proc(proc_t *proc, FuncPtr entry, uintptr_t args, const char* name) 
{
  thread_t* ret;

  if (proc == nullptr)
    return nullptr;

  const bool is_kernel = ((proc->m_flags & PROC_KERNEL) == PROC_KERNEL) ||
    ((proc->m_flags & PROC_DRIVER) == PROC_DRIVER);

  ret = create_thread(entry, args, name, is_kernel);

  if (proc_add_thread(proc, ret)) {
    destroy_thread(ret);
    return nullptr;
  }

  return ret;
}

/*!
 * @brief: Set the 
 */
void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t arg0, uintptr_t arg1) 
{
  THREAD_STATE_t state;

  /* Get the state of the current thread */
  thread_get_state(ptr, &state);

  if (state != NO_CONTEXT)
    return;

  contex_set_rip(&ptr->m_runtime_state.registers, (uintptr_t)entry, arg0, arg1);
}

/*!
 * @brief: Forcefully set the thread state
 */
void thread_set_state(thread_t *thread, THREAD_STATE_t state) 
{
  mutex_lock(thread->m_lock);

  /* Simply set the current state without any regard for the state of the stack xD */
  thread->state.stack[thread->state.current_state_idx] = state;

  mutex_unlock(thread->m_lock);
}

void thread_get_state(thread_t* thread, THREAD_STATE_t* state)
{
  mutex_lock(thread->m_lock);
  (void)thread_state_stack_peek(&thread->state, state);
  mutex_unlock(thread->m_lock);
}

void thread_push_state(thread_t* thread, THREAD_STATE_t state)
{
  mutex_lock(thread->m_lock);
  (void)thread_state_stack_push(&thread->state, state);
  mutex_unlock(thread->m_lock);
}

void thread_pop_state(thread_t* thread, THREAD_STATE_t* state)
{
  mutex_lock(thread->m_lock);
  (void)thread_state_stack_pop(&thread->state, state);
  mutex_unlock(thread->m_lock);
}

/*!
 * @brief: Destroy a thread
 */
ANIVA_STATUS destroy_thread(thread_t *thread) 
{
  proc_t* parent_proc;

  if (!thread)
    return ANIVA_FAIL;

  parent_proc = find_proc_by_id(thread->fid.proc_id);

  /* We need to do a bit more when we're actually still connected to a process */
  if (parent_proc) {

    Must(__kmem_dealloc(parent_proc->m_root_pd.m_root, parent_proc->m_resource_bundle, thread->m_kernel_stack_bottom, DEFAULT_STACK_SIZE));

    if (thread->m_user_stack_bottom)
      Must(__kmem_dealloc(parent_proc->m_root_pd.m_root, parent_proc->m_resource_bundle, thread->m_user_stack_bottom, DEFAULT_STACK_SIZE));

    /* Make sure we remove the thread from the processes queue */
    proc_remove_thread(parent_proc, thread);
  }

  /* Destroy the state stack */
  destroy_thread_state_stack(&thread->state);

  destroy_mutex(thread->m_lock);
  kfree((void*)thread->m_name);
  kfree(thread);
  return ANIVA_SUCCESS;
}

thread_t* get_generic_idle_thread() 
{
  ASSERT_MSG(__generic_idle_thread, "Tried to get generic idle thread before it was initialized");
  return __generic_idle_thread;
}

NAKED void common_thread_entry() {
  asm volatile (
    "popq %rdi \n" // our beautiful thread
    "popq %rsi \n" // ptr to its registers
    //"call thread_exit_init_state \n" // Call this to get bread
    // Go to the portal that might just take us to userland
    "jmp asm_common_irq_exit \n"
    );
}

/*!
 * @brief: Called every context switch
 */
extern void thread_enter_context(thread_t *to) 
{
  processor_t *cur_cpu;
  thread_t* prev_thread;
  THREAD_STATE_t state;

  /* Get the current state */
  thread_get_state(to, &state);

  /* Check that we are legal */
  ASSERT_MSG(state == RUNNABLE, "Tried to enter a thread that isn't RUNNABLE");

  cur_cpu = get_current_processor();
  prev_thread = get_previous_scheduled_thread();

  // NOTE: for correction purposes
  to->m_cpu = cur_cpu->m_cpu_num;

  /* Gib floats */
  store_fpu_state(&to->m_runtime_state.fpu_state);

  thread_set_state(to, RUNNING);


  // Only switch pagetables if we actually need to interchange between
  // them, otherwise thats just wasted tlb
  if (prev_thread->m_runtime_state.registers.cr3 == to->m_runtime_state.registers.cr3)
    return;

  /*
  printf("Switching from %s:%s (%d-%d) (%lld) to %s:%s (%d-%d) (%lld)\n",
      prev_thread->m_parent_proc->m_name, 
      prev_thread->m_name,
      prev_thread->m_parent_proc->m_id,
      prev_thread->m_tid,
      prev_thread->m_context.cr3,
      to->m_parent_proc->m_name,
      to->m_name, 
      to->m_parent_proc->m_id,
      to->m_tid,
      to->m_context.cr3
      );
      */

  kmem_load_page_dir(to->m_runtime_state.registers.cr3, false);
}

/*!
 * @brief: Called when a thread is created and is added to the scheduler for the first time
 */
ANIVA_STATUS thread_prepare_context(thread_t *thread) 
{
  uintptr_t rsp = thread->m_kernel_stack_top;
  THREAD_STATE_t state;

  thread_get_state(thread, &state);

  if (state != NO_CONTEXT)
    return ANIVA_FAIL;

  /* 
   * Stitch the exit function at the end of the thread stack 
   * NOTE: we can do this since STACK_PUSH first decrements the
   *       stack pointer and then it places the value
   */
  *(uintptr_t*)rsp = (uintptr_t)thread->f_exit;

  if ((thread->m_runtime_state.registers.cs & 3) != 0) {
    STACK_PUSH(rsp, uintptr_t, GDT_USER_DATA | 3);
    STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rsp);
  } else {
    STACK_PUSH(rsp, uintptr_t, 0);
    STACK_PUSH(rsp, uintptr_t, thread->m_kernel_stack_top);
  }

  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rflags);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.cs);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rip);

  // dummy irs_num and err_code
  STACK_PUSH(rsp, uintptr_t, 0);
  STACK_PUSH(rsp, uintptr_t, 0);

  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.r15);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.r14);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.r13);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.r12);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.r11);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.r10);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.r9);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.r8);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rax);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rbx);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rcx);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rdx);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rbp);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rsi);
  STACK_PUSH(rsp, uintptr_t, thread->m_runtime_state.registers.rdi);

  // ptr to registers_t struct above
  STACK_PUSH(rsp, uintptr_t, rsp + 8);

  thread->m_runtime_state.registers.rip = (uintptr_t) &common_thread_entry;
  thread->m_runtime_state.registers.rsp = rsp;
  thread->m_runtime_state.registers.rsp0 = thread->m_kernel_stack_top;
  thread->m_runtime_state.registers.cs = GDT_KERNEL_CODE;

  thread_set_state(thread, RUNNABLE);
  return ANIVA_SUCCESS;
}

// used to bootstrap the iret stub created in thread_prepare_context
// only on the first context switch
void bootstrap_thread_entries(thread_t* thread) 
{
  processor_t* c_processor;
  THREAD_STATE_t state;

  thread_get_state(thread, &state);

  /* Prepare that bitch */
  if (state == NO_CONTEXT)
    thread_prepare_context(thread);

  ASSERT(get_current_scheduling_thread() == thread);
  thread_set_state(thread, RUNNING);

  c_processor = get_current_processor();

  tss_entry_t *tss_ptr = &c_processor->m_tss;
  tss_ptr->iomap_base = sizeof(c_processor->m_tss);
  tss_ptr->rsp0l = thread->m_runtime_state.registers.rsp0 & 0xffffffff;
  tss_ptr->rsp0h = (thread->m_runtime_state.registers.rsp0 >> 32) & 0xffffffff;

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
    [new_rsp]"g"(thread->m_runtime_state.registers.rsp),
    [new_rip]"g"(thread->m_runtime_state.registers.rip),
    [thread]"d"(thread)
    : "memory"
  );
}

void thread_switch_context(thread_t* from, thread_t* to) 
{
  THREAD_STATE_t state;

  // FIXME: saving context is still wonky!!!
  // without saving the context it kinda works,
  // but it keeps running the function all over again.
  // since no state is saved, is just starts all over
  ASSERT_MSG(from, "Switched from NULL thread!");

  thread_get_state(from, &state);

  if (state == RUNNING)
    thread_set_state(from, RUNNABLE);

  ASSERT_MSG(get_previous_scheduled_thread() == from, "get_previous_scheduled_thread() is not the thread we came from!");

  tss_entry_t *tss_ptr = &get_current_processor()->m_tss;

  save_fpu_state(&from->m_runtime_state.fpu_state);

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
    "cli \n"
    "addq $8, %%rsp \n"
    THREAD_POP_REGS
    :
    [old_rbp]"=m"(from->m_runtime_state.registers.rbp),
    [old_rip]"=m"(from->m_runtime_state.registers.rip),
    [old_rsp]"=m"(from->m_runtime_state.registers.rsp),
    [tss_rsp0l]"=m"(tss_ptr->rsp0l),
    [tss_rsp0h]"=m"(tss_ptr->rsp0h),
      "=d"(to)
    :
    [new_rsp]"g"(to->m_runtime_state.registers.rsp),
    [new_rip]"g"(to->m_runtime_state.registers.rip),
    [base_stack_top]"g"(to->m_runtime_state.registers.rsp0),
    [thread]"d"(to)
    : "memory", "rbx"
  );
}

/*!
 * @brief: (NOTE: Not used atm) Called once right before we jump to the thread entry
 *
 * Would be used to do one-time initialization of a thread
 */
void thread_exit_init_state(thread_t *from, registers_t* regs) 
{
}

/*!
 * @brief: Block a thread
 *
 * This means the thread won't get scheduled until it is unblocked. Unblocking happens 
 * when a particular unblocking condition is met (i.e. a mutex is unlocked,
 * a blocking opperation finishes, ect.)
 *
 * TODO: Add blocking conditions
 */
void thread_block(thread_t* thread) 
{
  thread_t* this;
  THREAD_STATE_t state;

  this = get_current_scheduling_thread();

  mutex_lock(thread->m_lock);

  thread_get_state(thread, &state);

  /* Update the thread state to avoid weird behaviour on unblock */
  if (state == RUNNING)
    thread_set_state(thread, RUNNABLE);

  /* Push a blocked state onto the thread stack */
  thread_push_state(thread, BLOCKED);

  mutex_unlock(thread->m_lock);

  // FIXME: we do allow blocking other threads here, we need to
  // check if that comes with extra complications
  if (thread == this)
    scheduler_yield();
}

/*!
 * @brief: Return to the runnable threadpool
 */
void thread_unblock(thread_t* thread) 
{
  THREAD_STATE_t state;

  /* Get current state */
  thread_get_state(thread, &state);

  ASSERT_MSG(state == BLOCKED, "Tried to unblock a non-blocking thread!");

  /* We can't unblock ourselves, can we? */
  if (get_current_scheduling_thread() == thread) {
    kernel_panic("Wait, how did we unblock ourselves?");
  }

  /* Pop into the previous thread state */
  thread_pop_state(thread, NULL);
}

/*!
 * @brief: Put a thread into a sleeping state in which it won't get called by the scheduler
 *
 * TODO: What is the difference between sleeping and blocking?
 */
void thread_sleep(thread_t* thread) 
{
  thread_t* this;
  THREAD_STATE_t state;

  this = get_current_scheduling_thread();

  mutex_lock(thread->m_lock);

  thread_get_state(thread, &state);

  /* Update the thread state to avoid weird behaviour on wake */
  if (state == RUNNING)
    thread_set_state(thread, RUNNABLE);

  thread_push_state(thread, SLEEPING);

  mutex_unlock(thread->m_lock);

  if (thread == this)
    scheduler_yield();
}

/*!
 * @brief: Wake a thread from a sleeping state, so it can get scheduled again
 *
 * TODO: What is the difference between sleeping and blocking?
 */
void thread_wakeup(thread_t* thread) 
{
  THREAD_STATE_t state;

  /* Get current state */
  thread_get_state(thread, &state);

  if (state != SLEEPING)
    return;

  /* We can't unblock ourselves, can we? */
  if (get_current_scheduling_thread() == thread) {
    kernel_panic("Wait, how did we wake ourselves?");
  }

  /* Pop into the previous thread state */
  thread_pop_state(thread, NULL);
}

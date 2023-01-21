#include "scheduler.h"
#include "kmain.h"
#include "dev/debug/serial.h"
#include "mem/kmalloc.h"
#include "libk/string.h"

// how long this process takes to do it's shit
enum SCHED_FRAME_USAGE_LEVEL {
  SUPER_LOW = 0,
  LOW,
  MID,
  HIGH,
  SEVERE,
  MIGHT_BE_DEAD
};

enum SCHED_MODE {
  WAITING_FOR_FIRST_TICK = 0,
  SCHEDULING,
  PAUSED,
};

typedef struct sched_frame {
  proc_t* m_proc_to_schedule;
  size_t m_sched_time_left;
  size_t m_max_ticks;
  size_t m_max_async_task_threads;
  uint32_t m_scheduled_thread_index; // keeps track of which threads have already been scheduled
  
  enum SCHED_FRAME_USAGE_LEVEL m_fram_usage_lvl;
} sched_frame_t;

static proc_t* s_current_proc_in_scheduler;
static thread_t* s_current_thread_in_scheduler;
static list_t* s_sched_frames;
static spinlock_t* s_sched_switch_lock;
static atomic_ptr_t *s_no_schedule;
static enum SCHED_MODE s_sched_mode;

// --- inline functions --- TODO: own file?
static ALWAYS_INLINE sched_frame_t *create_sched_frame(proc_t* proc, enum SCHED_FRAME_USAGE_LEVEL level, size_t hard_max_async_task_threads);
static ALWAYS_INLINE void add_sched_frame(sched_frame_t* frame_ptr);
static ALWAYS_INLINE void remove_sched_frame(sched_frame_t* frame_ptr);
static ALWAYS_INLINE void push_sched_frame(sched_frame_t* frame_ptr);
static ALWAYS_INLINE sched_frame_t pop_sched_frame();
static ALWAYS_INLINE thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr);

// FIXME: create a place for this and remove test mark
static void test_kernel_thread_func() {
  println("entering test_kernel_thread_func");

  uintptr_t rsp;
  asm volatile (
    "movq %%rax, %0"
    : "=m"(rsp)
    :: "memory"
    );
  print("test_ktf rax: ");
  println(to_string(rsp));

  for (;;) {
    print("a");
  }
}
static void test1_func() {
  println("entering test1_func");

  for (;;) {
    print("b");
  }
}
static void test2_func() {
  println("entering test2_func");
  uintptr_t rsp;
  asm volatile (
    "movq %%rax, %0"
    : "=m"(rsp)
    :: "memory"
    );
  print("test2 rax: ");
  println(to_string(rsp));
  for (;;) {
    asm volatile (
      "movq %%rax, %0"
      : "=m"(rsp)
      :: "memory"
      );
    print(to_string(rsp));
  }
}

ANIVA_STATUS init_scheduler() {
  disable_interrupts();
  s_no_schedule = create_atomic_ptr();
  atomic_ptr_write(s_no_schedule, true);

  s_sched_frames = init_list();
  s_sched_switch_lock = init_spinlock();
  proc_t *kproc = create_kernel_proc(test_kernel_thread_func);

  thread_t *test1 = create_thread(test1_func, NULL, "test1", true);
  thread_t *test2 = create_thread(test2_func, NULL, "test2", true);

  println(to_string(test2->m_stack_top));

  thread_prepare_context(test1);
  thread_prepare_context(test2);

  list_append(kproc->m_threads, test1);
  list_append(kproc->m_threads, test2);

  // heap-allocate frame
  sched_frame_t *frame_ptr = create_sched_frame(kproc, LOW, 4);

  // push into the list
  push_sched_frame(frame_ptr);

  s_sched_mode = PAUSED;
  s_current_proc_in_scheduler = kproc;
  s_current_thread_in_scheduler = list_get(kproc->m_threads, 0);
  return ANIVA_SUCCESS;
}

// first scheduler entry
void start_scheduler(void) {

  Processor_t *current_processor = get_current_processor();

  sched_frame_t *frame_ptr = list_get(s_sched_frames, 0);

  // TODO: initial kernel worker thread
  // first switch to kernel thread until we are done with that thread.
  // after that mark the process as idle and start up other processes to
  // init further. these processes can be preempted
  thread_t *initial_thread = list_get(frame_ptr->m_proc_to_schedule->m_threads, 0);

  kContext_t *context_ptr = &initial_thread->m_context;

  tss_entry_t* tss = &current_processor->m_tss;
  tss->iomap_base = sizeof(current_processor->m_tss);
  tss->rsp0l = context_ptr->rsp0 & 0xffffffff;
  tss->rsp0h = context_ptr->rsp0 >> 32;

  atomic_ptr_write(s_no_schedule, false);

  s_current_thread_in_scheduler = initial_thread;
  s_current_proc_in_scheduler->m_prev_thread = initial_thread;

  s_sched_mode = WAITING_FOR_FIRST_TICK;
  //thread_load_context(initial_thread);
  for (;;) {

  }
}

void pick_next_thread_scheduler(void) {
  // get first frame (proc)
  sched_frame_t* frame = list_get(s_sched_frames, 0);

  // we should now either be in the kernel boot context, or in this mfs context
  thread_t *prev_thread = s_current_thread_in_scheduler;

  thread_t *next_thread = pull_runnable_thread_sched_frame(frame);

  if (next_thread == nullptr) {
    // TODO: find out what to do
    next_thread = frame->m_proc_to_schedule->m_idle_thread;
  }

  s_current_proc_in_scheduler->m_prev_thread = prev_thread;
  s_current_thread_in_scheduler = next_thread;
}

ANIVA_STATUS pause_scheduler() {

  CHECK_AND_DO_DISABLE_INTERRUPTS();
  lock_spinlock(s_sched_switch_lock);

  atomic_ptr_write(s_no_schedule, true);

  // peek first sched frame
  sched_frame_t *frame_ptr = list_get(s_sched_frames, 0);

  if (frame_ptr == nullptr) {
    // no sched frame yet
    unlock_spinlock(s_sched_switch_lock);
    CHECK_AND_TRY_ENABLE_INTERRUPTS();
    return ANIVA_FAIL;
  }

  // sched frame: when pausing the scheduler we want to be able to load this one
  // when we resume. Set it as current
  /*
  thread_t * first_thread = list_get(s_current_proc_in_scheduler->m_threads, 0);

  s_current_proc_in_scheduler = frame_ptr->m_proc_to_schedule;
  if (s_current_thread_in_scheduler != nullptr) {
    s_current_proc_in_scheduler->m_prev_thread = s_current_thread_in_scheduler;
  }
  s_current_thread_in_scheduler = (first_thread == nullptr ? s_current_proc_in_scheduler->m_idle_thread : first_thread);
  */

exit:
  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return ANIVA_SUCCESS;
}

void resume_scheduler(void) {

  // yes, schedule
  unlock_spinlock(s_sched_switch_lock);
  atomic_ptr_write(s_no_schedule, false);
}

void scheduler_cleanup() {

  uintptr_t rsp;
  asm volatile (
    "movq %%r8, %0"
    : "=m"(rsp)
    :: "memory"
    );
  print("sched_cleanup rax: ");
  println(to_string(rsp));

  enum SCHED_MODE prev_mode = s_sched_mode;
  s_sched_mode = SCHEDULING;

  thread_switch_context(s_current_proc_in_scheduler->m_prev_thread, s_current_thread_in_scheduler, prev_mode != WAITING_FOR_FIRST_TICK);

  kernel_panic("scheduler_cleanup");
}

ANIVA_STATUS sched_switch_context_to(thread_t *thread) {
    return ANIVA_FAIL;
}

registers_t *sched_tick(registers_t *registers_ptr) {

  println("sched_tick");
  enum SCHED_MODE prev_mode = s_sched_mode;

  if (atomic_ptr_load(s_no_schedule)) {
    return registers_ptr;
  }
  pause_scheduler();

  s_current_proc_in_scheduler->m_ticks_used++;

  thread_t *current_thread = s_current_thread_in_scheduler;
  println("got in");

  if (current_thread == nullptr) {
    println("no initial thread!");
    pick_next_thread_scheduler();
    return registers_ptr;
  }

  if (prev_mode != WAITING_FOR_FIRST_TICK) {
    current_thread->m_ticks_elapsed++;

    if (current_thread->m_ticks_elapsed >= current_thread->m_max_ticks) {
      current_thread->m_ticks_elapsed = 0;
      // we want to schedule a new thread at this point
      pick_next_thread_scheduler();
    }
  }
  resume_scheduler();

  println("returning");

  registers_ptr->rip = (uintptr_t)scheduler_cleanup;
  return registers_ptr;
}

thread_t *sched_next_thread() {
    return get_current_processor()->m_root_thread;
}

proc_t *sched_next_proc() {
    return nullptr;
}

void sched_next() {
}

void sched_safe_next() {
    Processor_t *current_processor = get_current_processor();

    if (!current_processor->m_being_handled_by_scheduler && current_processor->m_current_thread != nullptr) {
        sched_next();
    }
}

bool sched_has_next() {
    // TODO: check if we want another proc to have execution

    return (sched_next_thread() != nullptr);
}

void sched_first_switch_finished() {
}

ANIVA_STATUS sched_add_proc(proc_t *proc_ptr, proc_id id) {
  // kernelprocessID == 0
  if (id == 0) {

  }
}

ANIVA_STATUS sched_add_thead(thread_t *);

ANIVA_STATUS sched_remove_proc(proc_t *);

ANIVA_STATUS sched_remove_proc_by_id(proc_id);

ANIVA_STATUS sched_remove_thread(thread_t *);

ALWAYS_INLINE sched_frame_t *create_sched_frame(proc_t* proc, enum SCHED_FRAME_USAGE_LEVEL level, size_t hard_max_async_task_threads) {
  sched_frame_t *ptr = kmalloc(sizeof(sched_frame_t));
  ptr->m_fram_usage_lvl = level;
  ptr->m_max_async_task_threads = hard_max_async_task_threads;

  if (proc->m_requested_max_threads > ptr->m_max_async_task_threads) {
    proc->m_requested_max_threads = ptr->m_max_async_task_threads;
    if (proc->m_threads->m_length > ptr->m_max_async_task_threads) {
      kernel_panic("Thread limit of process somehow exceeded limit!");
    }
  }
  ptr->m_proc_to_schedule = proc;
  ptr->m_sched_time_left = SCHED_DEFAULT_PROC_START_TICKS;
  ptr->m_max_ticks = SCHED_DEFAULT_PROC_START_TICKS;
  return ptr;
}

ALWAYS_INLINE void add_sched_frame(sched_frame_t* frame_ptr) {
  list_append(s_sched_frames, frame_ptr);
}

ALWAYS_INLINE void remove_sched_frame(sched_frame_t* frame_ptr) {
  uintptr_t idx = 0;
  FOREACH(node, s_sched_frames) {
    if (node->data == (void*)frame_ptr) {
      list_remove(s_sched_frames, idx);
      return;
    }
    idx++;
  }
}
ALWAYS_INLINE void push_sched_frame(sched_frame_t* frame_ptr) {
  list_prepend(s_sched_frames, frame_ptr);
}
ALWAYS_INLINE sched_frame_t pop_sched_frame() {
  sched_frame_t frame = *(sched_frame_t*)list_get(s_sched_frames, 0);
  list_remove(s_sched_frames, 0);
  // should be stack-allocated at this point
  return frame;
}

ALWAYS_INLINE thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr) {

  const uint32_t prev_sched_thread_idx = ptr->m_scheduled_thread_index;
  ptr->m_scheduled_thread_index++;

  thread_t *next = list_get(ptr->m_proc_to_schedule->m_threads, ptr->m_scheduled_thread_index);;

  // plz be runnable
  while (next->m_current_state != RUNNABLE) {
    ptr->m_scheduled_thread_index++;

    // fuck
    if (ptr->m_scheduled_thread_index >= ptr->m_max_async_task_threads ||
        ptr->m_scheduled_thread_index >= ptr->m_proc_to_schedule->m_requested_max_threads) {
      ptr->m_scheduled_thread_index = 0;
    }

    // FIXME: wonky
    if (prev_sched_thread_idx == ptr->m_scheduled_thread_index) {
      //ptr->m_proc_to_schedule.
      return ptr->m_proc_to_schedule->m_idle_thread;
    }

    // pull next thread
    next = list_get(ptr->m_proc_to_schedule->m_threads, ptr->m_scheduled_thread_index);
  }

  // yay
  return next;
}
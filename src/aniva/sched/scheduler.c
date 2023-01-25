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

enum SCHED_FRAME_STATE {
  SCHED_FRAME_RUNNING = 0,
  SCHED_FRAME_IDLE,
  SCHED_FRAME_EXITING,
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

  const thread_t *m_initial_thread;
  enum SCHED_FRAME_STATE m_frame_state;
  enum SCHED_FRAME_USAGE_LEVEL m_fram_usage_lvl;
} sched_frame_t;

// --- static fields ---
static proc_t* s_current_proc_in_scheduler;
static thread_t *s_previous_thread;
static list_t* s_sched_frames;
static spinlock_t* s_sched_switch_lock;
static atomic_ptr_t *s_no_schedule;
static enum SCHED_MODE s_sched_mode;
static bool s_has_schedule_request;

// --- inline functions --- TODO: own file?
static ALWAYS_INLINE sched_frame_t *create_sched_frame(proc_t* proc, enum SCHED_FRAME_USAGE_LEVEL level, size_t hard_max_async_task_threads);
static ALWAYS_INLINE void add_sched_frame(sched_frame_t* frame_ptr);
static ALWAYS_INLINE void remove_sched_frame(sched_frame_t* frame_ptr);
static ALWAYS_INLINE void push_sched_frame(sched_frame_t* frame_ptr);
static ALWAYS_INLINE void send_sched_frame_to_back(uintptr_t idx);
static ALWAYS_INLINE sched_frame_t pop_sched_frame();
static void set_sched_frame_idle(sched_frame_t* frame_ptr);
static ALWAYS_INLINE thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr);
static ALWAYS_INLINE void set_previous_thread(thread_t* thread);

// FIXME: create a place for this and remove test mark
static void test_kernel_thread_func(uintptr_t arg) {
  println("entering test_kernel_thread_func");
  println(to_string(arg));
  uintptr_t rsp;
  asm volatile (
    "movq %%rax, %0 \n"
    : "=m"(rsp)
    :: "memory"
    );
  print("test rsp: ");
  println(to_string(rsp));

  for (;;){
  }
  //kernel_panic("test_kernel_thread_func panic");
}

ANIVA_STATUS init_scheduler() {
  disable_interrupts();
  s_no_schedule = create_atomic_ptr();
  atomic_ptr_write(s_no_schedule, true);
  s_sched_mode = WAITING_FOR_FIRST_TICK;
  s_has_schedule_request = false;

  s_sched_frames = init_list();
  s_sched_switch_lock = init_spinlock();

  proc_t *kproc = create_kernel_proc(test_kernel_thread_func, NULL);
  list_append(kproc->m_threads, create_thread_for_proc(kproc, test_kernel_thread_func, NULL, "hi"));

  sched_add_proc(kproc);

  s_current_proc_in_scheduler = kproc;
  set_current_handled_thread(nullptr);
  return ANIVA_SUCCESS;
}

// first scheduler entry
void start_scheduler(void) {

  sched_frame_t *frame_ptr = list_get(s_sched_frames, 0);

  // TODO: initial kernel worker thread
  // first switch to kernel thread until we are done with that thread.
  // after that mark the process as idle and start up other processes to
  // init further. these processes can be preempted
  thread_t *initial_thread = list_get(frame_ptr->m_proc_to_schedule->m_threads, 0);

  atomic_ptr_write(s_no_schedule, false);

  set_current_handled_thread(initial_thread);
  set_previous_thread(initial_thread);
  frame_ptr->m_initial_thread = initial_thread;

  s_sched_mode = WAITING_FOR_FIRST_TICK;

  // ensure interrupts enabled
  enable_interrupts();
  for (;;) {}
}

void pick_next_thread_scheduler(void) {
  // get first frame (proc)
  sched_frame_t* frame = list_get(s_sched_frames, 0);

  // we should now either be in the kernel boot context, or in this mfs context
  thread_t *prev_thread = get_current_scheduling_thread();

  thread_t *next_thread = pull_runnable_thread_sched_frame(frame);

  if (next_thread == nullptr) {
    // TODO: find out what to do
    next_thread = frame->m_proc_to_schedule->m_idle_thread;
  }

  set_previous_thread(prev_thread);
  set_current_handled_thread(next_thread);
}

ANIVA_STATUS pause_scheduler() {

  if (s_sched_mode == PAUSED) {
    return ANIVA_FAIL;
  }

  CHECK_AND_DO_DISABLE_INTERRUPTS();
  lock_spinlock(s_sched_switch_lock);

  atomic_ptr_write(s_no_schedule, true);

  s_sched_mode = PAUSED;

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

  if (s_sched_mode != PAUSED) {
    return;
  }
  // make sure the scheduler is in the right state
  s_sched_mode = SCHEDULING;

  // yes, schedule
  unlock_spinlock(s_sched_switch_lock);
  atomic_ptr_write(s_no_schedule, false);
}

void scheduler_cleanup() {

  s_has_schedule_request = false;

  sched_frame_t *frame = list_get(s_sched_frames, 0);
  thread_t *next_thread = get_current_scheduling_thread();
  thread_t *prev_thread = s_previous_thread;

  if (next_thread == frame->m_initial_thread && !next_thread->m_has_been_scheduled) {
    thread_enter_context_first_time(next_thread);
  }

  thread_switch_context(prev_thread, next_thread);

  print("entering thread: ");
  println(get_current_scheduling_thread()->m_name);
}

registers_t *sched_tick(registers_t *registers_ptr) {

  println("sched_tick");

  const enum SCHED_MODE prev_sched_mode = s_sched_mode;

  if (atomic_ptr_load(s_no_schedule) || !get_current_scheduling_thread()) {
    return registers_ptr;
  }
  pause_scheduler();

  sched_frame_t *current_frame = list_get(s_sched_frames, 0);

  if (s_sched_frames->m_length > 1) {
    current_frame->m_sched_time_left--;

    // FIXME: switch timings seem to be wonky
    if (current_frame->m_sched_time_left <= 0) {
      current_frame->m_sched_time_left = current_frame->m_max_ticks;

      println("Should have switched frames!");
      // switch_frames
      //resume_scheduler();
      //return registers_ptr;
      send_sched_frame_to_back(0);
      pick_next_thread_scheduler();
      s_has_schedule_request = true;
    }
  }

  s_current_proc_in_scheduler->m_ticks_used++;

  thread_t *current_thread = get_current_scheduling_thread();

  if (current_thread == nullptr) {
    println("no initial thread!");
    pick_next_thread_scheduler();
    return registers_ptr;
  }

  if (prev_sched_mode != WAITING_FOR_FIRST_TICK && !s_has_schedule_request) {
    current_thread->m_ticks_elapsed++;

    if (current_thread->m_ticks_elapsed >= current_thread->m_max_ticks) {
      current_thread->m_ticks_elapsed = 0;
      // we want to schedule a new thread at this point
      pick_next_thread_scheduler();
      s_has_schedule_request = true;
    }
  } else {
    s_has_schedule_request = true;
  }

  resume_scheduler();

  if (s_has_schedule_request) {
    scheduler_cleanup();
  }
  return registers_ptr;
}

// TODO: check safety
ANIVA_STATUS sched_add_proc(proc_t *proc_ptr) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();

  sched_frame_t *frame_ptr = create_sched_frame(proc_ptr, LOW, proc_ptr->m_requested_max_threads);

  add_sched_frame(frame_ptr);

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return ANIVA_SUCCESS;
}

ANIVA_STATUS sched_remove_proc(proc_t *);

ANIVA_STATUS sched_remove_proc_by_id(proc_id);

ANIVA_STATUS sched_remove_thread(thread_t *);

ALWAYS_INLINE sched_frame_t *create_sched_frame(proc_t* proc, enum SCHED_FRAME_USAGE_LEVEL level, size_t hard_max_async_task_threads) {
  sched_frame_t *ptr = kmalloc(sizeof(sched_frame_t));
  ptr->m_fram_usage_lvl = level;
  ptr->m_frame_state = SCHED_FRAME_RUNNING;
  ptr->m_max_async_task_threads = hard_max_async_task_threads;

  if (proc->m_requested_max_threads > ptr->m_max_async_task_threads) {
    proc->m_requested_max_threads = ptr->m_max_async_task_threads;
    if (proc->m_threads->m_length > ptr->m_max_async_task_threads) {
      kernel_panic("Thread limit of process somehow exceeded limit!");
    }
  }
  ptr->m_initial_thread = list_get(proc->m_threads, 0);
  ptr->m_proc_to_schedule = proc;
  ptr->m_sched_time_left = SCHED_FRAME_DEFAULT_START_TICKS;
  ptr->m_max_ticks = SCHED_FRAME_DEFAULT_START_TICKS;
  return ptr;
}

ALWAYS_INLINE void add_sched_frame(sched_frame_t* frame_ptr) {
  list_append(s_sched_frames, frame_ptr);
}

// difference between add and insert is the fact that
// insert requires the scheduler to wait with scheduling
// until we have added the frame
void insert_sched_frame(sched_frame_t* frame_ptr) {
  if (s_sched_mode == PAUSED) {
    add_sched_frame(frame_ptr);
    return;
  }
  pause_scheduler();
  add_sched_frame(frame_ptr);
  resume_scheduler();
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

static ALWAYS_INLINE void send_sched_frame_to_back(uintptr_t idx) {
  if (s_sched_frames->m_length == 0) {
    kernel_panic("no sched frame to send to back!");
  }

  if (s_sched_mode != PAUSED) {
    pause_scheduler();
  }
  sched_frame_t *frame = list_get(s_sched_frames, idx);
  list_remove(s_sched_frames, idx);
  list_append(s_sched_frames, frame);
  resume_scheduler();
}

ALWAYS_INLINE thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr) {

  const uint32_t prev_sched_thread_idx = ptr->m_scheduled_thread_index;
  ptr->m_scheduled_thread_index++;

  thread_t *next = list_get(ptr->m_proc_to_schedule->m_threads, ptr->m_scheduled_thread_index);

  if (next == nullptr) {
    next = list_get(ptr->m_proc_to_schedule->m_threads, 0);
    ptr->m_scheduled_thread_index = 0;
  }

  // plz be runnable
  while (!next || next->m_current_state != RUNNABLE) {
    ptr->m_scheduled_thread_index++;

    // fuck
    if (ptr->m_scheduled_thread_index >= ptr->m_max_async_task_threads ||
        ptr->m_scheduled_thread_index >= ptr->m_proc_to_schedule->m_requested_max_threads) {
      ptr->m_scheduled_thread_index = 0;
    }

    if (prev_sched_thread_idx == ptr->m_scheduled_thread_index) {
      set_sched_frame_idle(ptr);
      return nullptr;
    }

    // TODO
    switch (next->m_current_state) {
      case BLOCKED:
      default:
        break;
    }

    // pull next thread
    next = list_get(ptr->m_proc_to_schedule->m_threads, ptr->m_scheduled_thread_index);
  }

  // yay
  return next;
}

// doing this means we have two pointers that point to the
// same memory address (thread) idk if this might get ugly
// TODO: sync?
void set_current_handled_thread(thread_t* thread) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  Processor_t *current_processor = get_current_processor();
  current_processor->m_current_thread = thread;
  CHECK_AND_TRY_ENABLE_INTERRUPTS();
}

void set_sched_frame_idle(sched_frame_t* frame_ptr) {
  frame_ptr->m_frame_state = SCHED_FRAME_IDLE;
  frame_ptr->m_max_ticks = 20;
}

ALWAYS_INLINE void set_previous_thread(thread_t* thread) {
  s_current_proc_in_scheduler->m_prev_thread = thread;
  s_previous_thread = thread;
}

thread_t *get_current_scheduling_thread() {
  return (thread_t*)read_gs(GET_OFFSET(Processor_t , m_current_thread));
}
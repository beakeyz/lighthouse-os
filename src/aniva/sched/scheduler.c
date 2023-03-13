#include "scheduler.h"
#include "dev/framebuffer/framebuffer.h"
#include "entry/entry.h"
#include "dev/debug/serial.h"
#include <mem/heap.h>
#include "libk/error.h"
#include "libk/string.h"
#include "interupts/interupts.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "proc/socket.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/asm_specifics.h"
#include "system/processor/processor.h"
#include <mem/heap.h>

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
static list_t* s_sched_frames;
//static spinlock_t* s_sched_switch_lock;
//static atomic_ptr_t *s_no_schedule;
static mutex_t* s_sched_mutex;
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
static USED thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr);
static ALWAYS_INLINE void set_previous_thread(thread_t* thread);

ANIVA_STATUS init_scheduler() {
  disable_interrupts();
  s_sched_mode = PAUSED;
  s_has_schedule_request = false;

  s_sched_frames = init_list();
  s_sched_mutex = create_mutex(0);

  set_current_handled_thread(nullptr);

  return ANIVA_SUCCESS;
}

// first scheduler entry
void start_scheduler(void) {

  sched_frame_t *frame_ptr = list_get(s_sched_frames, 0);

  if (frame_ptr == nullptr) {
    return;
  }

  //if (is_kernel_proc(frame_ptr->m_proc_to_schedule) && sched_get_kernel_proc() == nullptr) {
  //  get_current_processor()->m_kernel_process = frame_ptr->m_proc_to_schedule;
  //} else {
    // we might have just tried to initialize the kernel proc twice for this thread...
    // or sm else thats just wrong
    // FIXME:
  //}

  // let's jump into the idle thread initially, so we can then
  // resume to the thread-pool when we leave critical sections
  thread_t *initial_thread = frame_ptr->m_proc_to_schedule->m_idle_thread;

  //atomic_ptr_write(s_no_schedule, false);

  set_current_handled_thread(initial_thread);
  set_previous_thread(initial_thread);
  frame_ptr->m_initial_thread = initial_thread;

  s_sched_mode = SCHEDULING;

  //disable_interrupts();
  // ensure interrupts enabled
  //enable_interrupts();
  //for (;;) {}
  bootstrap_thread_entries(initial_thread);
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

  mutex_lock(s_sched_mutex);

  s_sched_mode = PAUSED;

  // FIXME: wtf?
  // peek first sched frame
  sched_frame_t *frame_ptr = list_get(s_sched_frames, 0);

  if (frame_ptr == nullptr) {
    // no sched frame yet
    return ANIVA_FAIL;
  }

  return ANIVA_SUCCESS;
}

void resume_scheduler(void) {

  if (s_sched_mode != PAUSED && !mutex_is_locked(s_sched_mutex)) {
    return;
  }

  // make sure the scheduler is in the right state
  s_sched_mode = SCHEDULING;

  // yes, schedule
  mutex_unlock(s_sched_mutex);
}

void scheduler_yield() {
  disable_interrupts();
  // invoke the scheduler early
  scheduler_try_invoke();
  // prepare the next thread
  pick_next_thread_scheduler();

  Processor_t *current = get_current_processor();
  thread_t *current_thread = get_current_scheduling_thread();

  ASSERT_MSG(current_thread != nullptr, "trying to yield the scheduler while not having a current scheduling thread!");

  // in this case we don't have to wait for us to exit a
  // critical CPU section, since we are not being interrupted at all
  if (current->m_irq_depth == 0 && atomic_ptr_load(current->m_critical_depth) == 0) {
    scheduler_try_execute();
  }
}

ErrorOrPtr scheduler_try_execute() {

  Processor_t *current = get_current_processor();
  ASSERT_MSG(current, "Could not get current processor while trying to calling scheduler")
  ASSERT_MSG(current->m_irq_depth == 0, "Trying to call scheduler while in irq");

  if (atomic_ptr_load(current->m_critical_depth) != 0) {
    return Error();
  }

  if (!s_has_schedule_request)
    return Error();

  s_has_schedule_request = false;

  sched_frame_t *frame = list_get(s_sched_frames, 0);
  thread_t *next_thread = get_current_scheduling_thread();
  thread_t *prev_thread = frame->m_proc_to_schedule->m_prev_thread;

  //if (next_thread == frame->m_initial_thread && !next_thread->m_has_been_scheduled) {
  //  bootstrap_thread_entries(next_thread);
  //}

  thread_switch_context(prev_thread, next_thread);
  return Success(0);
}

// TODO: ?
ErrorOrPtr scheduler_try_invoke() {
  s_has_schedule_request = true;
  return Success(0);
}

registers_t *sched_tick(registers_t *registers_ptr) {

  const enum SCHED_MODE prev_sched_mode = s_sched_mode;

  if (mutex_is_locked(s_sched_mutex)) {
    return registers_ptr;
  }

  if (!get_current_scheduling_thread()) {
    return registers_ptr;
  }

  /*
  if (pause_scheduler() == ANIVA_FAIL) {
    return registers_ptr;
  }
  */

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
      scheduler_try_invoke();
    }
  }

  current_frame->m_proc_to_schedule->m_ticks_used++;

  thread_t *current_thread = get_current_scheduling_thread();

  if (current_thread == nullptr) {
    println("no initial thread!");
    pick_next_thread_scheduler();
    return registers_ptr;
  }

  current_thread->m_ticks_elapsed++;

  if (current_thread->m_ticks_elapsed >= current_thread->m_max_ticks) {
    current_thread->m_ticks_elapsed = 0;
    // we want to schedule a new thread at this point
    pick_next_thread_scheduler();
    scheduler_try_invoke();
  }

  //resume_scheduler();
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
  ptr->m_scheduled_thread_index = 0;
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

/*
 * TODO: redo this function, as it is way too messy and
 * should be as optimal as possible
 * perhaps we can use a queue or something (it's unclear to me which ds works best here...)
 */
thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr) {
  const uint32_t prev_sched_thread_idx = ptr->m_scheduled_thread_index;
  const thread_t *current_thread = get_current_scheduling_thread();
  list_t *thread_list_ptr = ptr->m_proc_to_schedule->m_threads;

  if (thread_list_ptr->m_length == 0) {
    return nullptr;
  }

  uintptr_t current_idx = prev_sched_thread_idx + 1;
  thread_t* next_thread = nullptr;

  while (true) {

    next_thread = list_get(thread_list_ptr, current_idx);

    if (next_thread == nullptr) {
      current_idx = 0;
      continue;
    }

    bool found = false;

    switch (next_thread->m_current_state) {
      case INVALID:
      case NO_CONTEXT:
        // TODO: there is an invalid thread in the pool, handle it.
        break;
      case SLEEPING:
      case RUNNABLE:
        // potential good thread so TODO: make an algorithm that chooses the optimal thread here
        // another TODO: we need to figure out how to handle sleeping threads (i.e. sockets waiting for packets)
        found = true;
        break;
      case DYING:
        // TODO: this is an intermediate solution, we need actual thread death procedures
        thread_set_state(next_thread, DEAD);
        break;
      case DEAD:
        destroy_thread(next_thread);
        list_remove(thread_list_ptr, current_idx);
        break;
      case STOPPED:
      case BLOCKED:
        // TODO: let's figure out how to handle blocked threads (so waiting on IO or some shit)
        // since we don't have any real IO yet this is kind of a placeholder lmao
        break;
      default:
        break;
    }

    if (found) {
      break;
    }

    // we went around the loop, and found nothing to run =/
    if (current_idx == prev_sched_thread_idx) {
      return nullptr;
    }

    current_idx++;
  }

  // update the scheduler
  ptr->m_scheduled_thread_index = current_idx;

  return next_thread;
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
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  sched_frame_t *frame = list_get(s_sched_frames, 0);
  Processor_t *current_processor = get_current_processor();
  frame->m_proc_to_schedule->m_prev_thread = thread;
  current_processor->m_previous_thread = thread;
  CHECK_AND_TRY_ENABLE_INTERRUPTS();
}

thread_t *get_current_scheduling_thread() {
  return (thread_t*)read_gs(GET_OFFSET(Processor_t , m_current_thread));
}

thread_t *get_previous_scheduled_thread() {
  return (thread_t*)read_gs(GET_OFFSET(Processor_t , m_previous_thread));
}

proc_t* sched_get_kernel_proc() {
  return (proc_t*)read_gs(GET_OFFSET(Processor_t, m_kernel_process));
}

void set_kernel_proc(proc_t* proc) {
  Processor_t* current = get_current_processor();
  if (current->m_kernel_process != nullptr) {
    return;
  }
  current->m_kernel_process = proc;
}

bool sched_can_schedule() {
  return (!mutex_is_locked(s_sched_mutex));
}

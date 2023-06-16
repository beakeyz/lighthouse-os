#include "scheduler.h"
#include "dev/framebuffer/framebuffer.h"
#include "entry/entry.h"
#include "dev/debug/serial.h"
#include <mem/heap.h>
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/string.h"
#include "interrupts/interrupts.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "proc/socket.h"
#include "proc/thread.h"
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
  UNINITIALIZED_SCHED = 0,
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
static ALWAYS_INLINE void add_sched_frame_for_next_execute(sched_frame_t* frame_ptr);
static ALWAYS_INLINE ErrorOrPtr remove_sched_frame(sched_frame_t* frame_ptr);
static ALWAYS_INLINE void push_sched_frame(sched_frame_t* frame_ptr);
static ALWAYS_INLINE void send_sched_frame_to_back(uintptr_t idx);
static ALWAYS_INLINE sched_frame_t* find_sched_frame(proc_id proc);
static ALWAYS_INLINE sched_frame_t pop_sched_frame();
static void set_sched_frame_idle(sched_frame_t* frame_ptr);
static USED thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr);
static ALWAYS_INLINE void set_previous_thread(thread_t* thread);
static ALWAYS_INLINE void set_current_proc(proc_t* proc);
static void should_proc_die(proc_t* proc);

ANIVA_STATUS init_scheduler() {
  disable_interrupts();
  s_sched_mode = UNINITIALIZED_SCHED;
  s_has_schedule_request = false;

  s_sched_frames = init_list();
  s_sched_mutex = create_mutex(0);

  set_current_handled_thread(nullptr);
  set_current_proc(nullptr);

  return ANIVA_SUCCESS;
}

// first scheduler entry
void start_scheduler(void) {

  sched_frame_t *frame_ptr = list_get(s_sched_frames, 0);

  if (frame_ptr == nullptr) {
    return;
  }

  // let's jump into the idle thread initially, so we can then
  // resume to the thread-pool when we leave critical sections
  thread_t *initial_thread = frame_ptr->m_proc_to_schedule->m_init_thread;

  //atomic_ptr_write(s_no_schedule, false);

  set_current_proc(frame_ptr->m_proc_to_schedule);
  set_current_handled_thread(initial_thread);
  set_previous_thread(nullptr);
  frame_ptr->m_initial_thread = initial_thread;

  s_sched_mode = SCHEDULING;

  bootstrap_thread_entries(initial_thread);
}

static void pick_next_process_scheduler() {
  uintptr_t idx = 0;
  sched_frame_t* current;

  while (idx++ < s_sched_frames->m_length+1) {
    send_sched_frame_to_back(0);

    current = list_get(s_sched_frames, 0);

    if(!(current->m_proc_to_schedule->m_flags & PROC_FINISHED))
      break;
    
  }
}

/*
 * Try to fetch another thread for us to schedule.
 * If this current process is in idle mode, we just 
 * schedule the idle thread
 */
ErrorOrPtr pick_next_thread_scheduler(void) {
  // get first frame (proc)
  sched_frame_t* frame = list_get(s_sched_frames, 0);

  if (!frame)
    return Error();

  /* Check if this frame is done */
  while (!proc_can_schedule(frame->m_proc_to_schedule)) {
    /* Kill the proc, TODO: do that somewhere async, so it doesn't block the scheduler */
    //destroy_proc(frame->m_proc_to_schedule);

    remove_sched_frame(frame);

    frame = list_get(s_sched_frames, 0);
  }

  set_current_proc(frame->m_proc_to_schedule);

  // we should now either be in the kernel boot context, or in this mfs context
  thread_t *prev_thread = get_current_scheduling_thread();

  if (frame->m_proc_to_schedule->m_flags & PROC_IDLE) {
    kernel_panic("FIXME: Trying to set idlethread");

    println(prev_thread->m_name);
    set_previous_thread(frame->m_proc_to_schedule->m_idle_thread);
    set_current_handled_thread(frame->m_proc_to_schedule->m_idle_thread);

    return Success(0);
  }

  thread_t *next_thread = pull_runnable_thread_sched_frame(frame);

  /* There are no threads left. We can safely kill this process */
  if (next_thread == nullptr) {
    println("No next thread");
    return Error();
  }

  set_previous_thread(prev_thread);
  set_current_handled_thread(next_thread);

  return Success(0);
}

ANIVA_STATUS pause_scheduler() {
  if (s_sched_mode == UNINITIALIZED_SCHED)
    return ANIVA_FAIL;

  if (!s_sched_mutex)
    return ANIVA_FAIL;

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
  if (!s_sched_mutex)
    return;

  if (s_sched_mode != PAUSED && !mutex_is_locked(s_sched_mutex)) {
    return;
  }

  // make sure the scheduler is in the right state
  s_sched_mode = SCHEDULING;

  // yes, schedule
  mutex_unlock(s_sched_mutex);
}

void scheduler_yield() {
  ASSERT_MSG(s_sched_frames, "Tried to yield to an uninitialized scheduler!");
  ASSERT_MSG(s_sched_mode != UNINITIALIZED_SCHED, "Tried to yield to an unstarted scheduler!");

  CHECK_AND_DO_DISABLE_INTERRUPTS();
  // invoke the scheduler early
  scheduler_try_invoke();
  // prepare the next thread
  if (pick_next_thread_scheduler().m_status != ANIVA_SUCCESS) {
    kernel_panic("TODO: terminate the process which has no threads left!");
  }

  Processor_t *current = get_current_processor();
  thread_t *current_thread = get_current_scheduling_thread();

  ASSERT_MSG(current_thread != nullptr, "trying to yield the scheduler while not having a current scheduling thread!");

  // in this case we don't have to wait for us to exit a
  // critical CPU section, since we are not being interrupted at all
  if (current->m_irq_depth == 0 && atomic_ptr_load(current->m_critical_depth) == 0) {
    scheduler_try_execute();
  }

  /* Match the disable interrupts call above */
  CHECK_AND_TRY_ENABLE_INTERRUPTS();
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

  thread_switch_context(prev_thread, next_thread);
  return Success(0);
}

// TODO: ?
ErrorOrPtr scheduler_try_invoke() {
  s_has_schedule_request = true;
  return Success(0);
}

/*
 * Let's just pagefault is we don't have a process lmao
 */
void sched_idle_current_process() {
  disable_interrupts();

  sched_frame_t* current_frame = list_get(s_sched_frames, 0);

  current_frame->m_proc_to_schedule->m_flags |= PROC_IDLE;

  scheduler_yield();
}

/*
 * Let's just pagefault is we don't have a process lmao
 */
void sched_wake_current_process() {
  disable_interrupts();

  sched_frame_t* current_frame = list_get(s_sched_frames, 0);

  current_frame->m_proc_to_schedule->m_flags &= ~PROC_IDLE;

  scheduler_yield();
}

registers_t *sched_tick(registers_t *registers_ptr) {

  if (s_sched_mode == UNINITIALIZED_SCHED)
    return registers_ptr;

  if (mutex_is_locked(s_sched_mutex))
    return registers_ptr;

  if (!get_current_scheduling_thread())
    return registers_ptr;

  sched_frame_t *current_frame = list_get(s_sched_frames, 0);

  if (current_frame->m_proc_to_schedule->m_flags & PROC_IDLE) {
    Must(pick_next_thread_scheduler());

    scheduler_try_invoke();
    return registers_ptr;
  }

  if (s_sched_frames->m_length > 1) {

    // FIXME: switch timings seem to be wonky
    if (current_frame->m_sched_time_left <= 0) {
      current_frame->m_sched_time_left = current_frame->m_max_ticks;

      // switch_frames
      println("Should have switched frames!");
      //send_sched_frame_to_back(0);
      pick_next_process_scheduler();
      Must(pick_next_thread_scheduler());

      /* Debug */
      println(get_current_proc()->m_name);
      println(get_current_scheduling_thread()->m_name);
      println(to_string((uintptr_t)get_current_scheduling_thread()->m_real_entry));

      scheduler_try_invoke();
      return registers_ptr;
    }
    current_frame->m_sched_time_left--;
  }

  current_frame->m_proc_to_schedule->m_ticks_used++;

  thread_t *current_thread = get_current_scheduling_thread();

  ASSERT_MSG(current_thread, "No initial thread!");
  /*
  if (current_thread == nullptr) {
    println("no initial thread!");
    pick_next_thread_scheduler();
    return registers_ptr;
  }
  */

  current_thread->m_ticks_elapsed++;

  if (current_thread->m_ticks_elapsed >= current_thread->m_max_ticks) {
    current_thread->m_ticks_elapsed = 0;
    // we want to schedule a new thread at this point
    if (pick_next_thread_scheduler().m_status != ANIVA_SUCCESS) {
      // TODO: we might try to spawn another eternal process
      kernel_panic("Could not pick another thread to schedule!");
    }
    scheduler_try_invoke();
  }

  //resume_scheduler();
  return registers_ptr;
}

ANIVA_STATUS sched_add_proc(proc_t *proc_ptr) {

  /* If the scheduler is busy, we simply fuck off */
  if (mutex_is_locked(s_sched_mutex))
    return ANIVA_FAIL;

  /* Yay, we can alter scheduler behaviour */
  mutex_lock(s_sched_mutex);

  sched_frame_t *frame_ptr = create_sched_frame(proc_ptr, LOW, proc_ptr->m_requested_max_threads);

  add_sched_frame(frame_ptr);

  mutex_unlock(s_sched_mutex);
  return ANIVA_SUCCESS;
}

ErrorOrPtr sched_add_priority_proc(proc_t* proc, bool reschedule) {

  /* If the scheduler is busy, we simply fuck off */
  if (mutex_is_locked(s_sched_mutex))
    return Error();

  /* Yay, we can alter scheduler behaviour */
  mutex_lock(s_sched_mutex);

  sched_frame_t *frame_ptr = create_sched_frame(proc, LOW, proc->m_requested_max_threads);

  add_sched_frame_for_next_execute(frame_ptr);

  if (reschedule) {
    sched_frame_t* current_frame = list_get(s_sched_frames, 0);

    // Drain time left, so it gets rescheduled on the next tick
    current_frame->m_sched_time_left = 0;

    /* We'll have to unlock the mutex here also, to avoid nasty shit */
    mutex_unlock(s_sched_mutex);
    scheduler_yield();

    return Success(0);
  }

  mutex_unlock(s_sched_mutex);
  return Success(0);
}

ANIVA_STATUS sched_remove_proc(proc_t *proc) {
  if (!proc)
    return ANIVA_FAIL;

  return sched_remove_proc_by_id(proc->m_id);
}

/*
 * We require the scheduler to be locked by this 
 * point, which generaly means it is paused
 */
ANIVA_STATUS sched_remove_proc_by_id(proc_id id) {

  if (!mutex_is_locked(s_sched_mutex))
    return ANIVA_FAIL;

  sched_frame_t* frame = find_sched_frame(id);

  if (!frame)
    return ANIVA_FAIL;

  if (remove_sched_frame(frame).m_status != ANIVA_SUCCESS)
    return ANIVA_FAIL;

  return ANIVA_SUCCESS;
}

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

ALWAYS_INLINE void add_sched_frame_for_next_execute(sched_frame_t* frame_ptr) {
  // Place the new schedframe right behind the first (now running)
  // sched frame, so it is next in line for execution
  list_append_after(s_sched_frames, frame_ptr, 0);
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

/*
 * We should find a better way to index processes based on priority, rather than 
 * just throwing them in a pseudo-queue (which is really a list lmao)
 */
ALWAYS_INLINE ErrorOrPtr remove_sched_frame(sched_frame_t* frame_ptr) {

  TRY(result, list_indexof(s_sched_frames, frame_ptr));

  if (!list_remove(s_sched_frames, result)) {
    return Error();
  }

  return Success(0);
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

  sched_frame_t *frame = list_get(s_sched_frames, idx);
  list_remove(s_sched_frames, idx);
  list_append(s_sched_frames, frame);
  //resume_scheduler();
}

/*
 * We just linear search through the list, since it does not really hurt 
 * performance with a small amount of processes. When this number grows though,
 * we might need to look into sorting processes based on proc_id, and the 
 * preforming a binary search, so TODO
 */
static ALWAYS_INLINE sched_frame_t* find_sched_frame(proc_id procid) {

  FOREACH(i, s_sched_frames) {
    sched_frame_t* frame = i->data;

    if (frame->m_proc_to_schedule->m_id == procid) {
      return frame;
    }
  }
  return nullptr;
}

#define SCHED_MAX_PULL_CYCLES 3

/*
 * TODO: redo this function, as it is way too messy and
 * should be as optimal as possible
 * perhaps we can use a queue or something (it's unclear to me which ds works best here...)
 *
 * Returning nullptr means that this thread does not have any threads left. Let's just kill it at that point...
 */
thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr) {
  const uint32_t prev_sched_thread_idx = ptr->m_scheduled_thread_index;
  const thread_t *current_thread = get_current_scheduling_thread();
  proc_t* proc = ptr->m_proc_to_schedule;
  list_t *thread_list_ptr = proc->m_threads;

  if (thread_list_ptr->m_length == 0) {
    return nullptr;
  }

  uintptr_t cycles = 0;
  uintptr_t unrunnable_threads = 0;
  uintptr_t current_idx = prev_sched_thread_idx + 1;
  thread_t* next_thread = nullptr;

  /* Clear the flag and run initializer */
  if ((proc->m_flags & PROC_UNRUNNED) == PROC_UNRUNNED) {
    proc->m_flags &= ~PROC_UNRUNNED;

    /* We require the initial thread to be in the threads list */
    ptr->m_scheduled_thread_index = Must(list_indexof(thread_list_ptr, proc->m_init_thread)) + 1;

    return proc->m_init_thread;
  }

  /* Only one entry. Just kinda run it ig */
  if (thread_list_ptr->m_length == 1) {
    thread_t* core_thread = thread_list_ptr->head->data;

    if (core_thread->m_current_state == RUNNABLE)
      return core_thread;

    if (core_thread->m_current_state == RUNNING) {
      thread_set_state(core_thread, RUNNABLE);

      return core_thread;
    }

    println("WARNING: only thread in proc is not runnable or running =(");
  }

  while (true) {

    next_thread = list_get(thread_list_ptr, current_idx);

    if (!next_thread && cycles > SCHED_MAX_PULL_CYCLES) {
      /* If there are no threads left, this process is just dead */
      if (thread_list_ptr->m_length == 0) {
        return nullptr;
      }

      /*
       * FIXME: in stead of switching to an idle thread, we should mark the process as idle, and skip it 
       * from this point onwards until it gets woken up
       */
      next_thread = ptr->m_proc_to_schedule->m_idle_thread;

      kernel_panic("TODO: the scheduler failed to pull a thread from the process so we should do sm about it!");

      /* FIXME: we can prob allow this, because we can just kill the process */
      ASSERT_MSG(next_thread, "FATAL: Tried to idle a process without it having an idle-thread");
      ASSERT_MSG(next_thread->m_current_state == RUNNABLE, "FATAL: the idle thread has not been marked runnable for some reason");
      break;
    } 

    if (!next_thread) {
      current_idx = 0;
      cycles++;
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

        /* Let's recurse here, in order to update our loop */
        return pull_runnable_thread_sched_frame(ptr);
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

    current_idx++;
  }

  ASSERT_MSG(next_thread, "FATAL: Tried to pick a null-thread to schedule!");

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

static ALWAYS_INLINE void set_current_proc(proc_t* proc) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();

  Processor_t *current_processor = get_current_processor();
  current_processor->m_current_proc = proc;

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
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

proc_t* get_current_proc() {
  return (proc_t*)read_gs(GET_OFFSET(Processor_t , m_current_proc));
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

#include "scheduler.h"
#include "kmain.h"
#include "dev/debug/serial.h"
#include "mem/kmalloc.h"

#define SCHED_DEFAULT_PROC_START_TICKS 400

// how long this process takes to do it's shit
enum SCHED_FRAME_USAGE_LEVEL {
  SUPER_LOW,
  LOW,
  MID,
  HIGH,
  SEVERE,
  MIGHT_BE_DEAD
};

typedef struct sched_frame {
  proc_t* m_proc_to_schedule;
  size_t m_sched_time_left;
  size_t m_max_ticks;
  size_t m_max_async_task_threads;
  
  enum SCHED_FRAME_USAGE_LEVEL m_fram_usage_lvl;
} sched_frame_t;

static proc_t* s_current_proc_in_scheduler;
static list_t* s_sched_frames;
static spinlock_t* s_sched_switch_lock;

// --- inline functions --- TODO: own file?
ALWAYS_INLINE sched_frame_t *create_sched_frame(proc_t* proc, enum SCHED_FRAME_USAGE_LEVEL level, size_t hard_max_async_task_threads);
ALWAYS_INLINE void add_sched_frame(sched_frame_t* frame_ptr);
ALWAYS_INLINE void remove_sched_frame(sched_frame_t* frame_ptr);
ALWAYS_INLINE void push_sched_frame(sched_frame_t* frame_ptr);
ALWAYS_INLINE sched_frame_t pop_sched_frame();

// FIXME: create a place for this and remove test mark
static void test_kernel_thread_func() {
  println("entering test_kernel_thread_func");
}


ANIVA_STATUS init_scheduler() {
  s_sched_frames = init_list();
  proc_t *kproc = create_kernel_proc(test_kernel_thread_func);

  // heap-allocate frame
  sched_frame_t *frame_ptr = create_sched_frame(kproc, LOW, 4);

  // push into the list
  push_sched_frame(frame_ptr);


  return ANIVA_FAIL;
}

void resume_scheduler(void) {
  // get first frame (proc)
  sched_frame_t* frame = list_get(s_sched_frames, 0);

  // TODO

}

ANIVA_STATUS pause_scheduler() {

}

void scheduler_cleanup() {
}

ANIVA_STATUS sched_switch_context_to(thread_t *thread) {
    return ANIVA_FAIL;
}

void sched_tick(registers_t *);

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
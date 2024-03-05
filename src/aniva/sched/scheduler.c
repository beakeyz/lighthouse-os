#include "scheduler.h"
#include <mem/heap.h>
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "irq/interrupts.h"
#include "logging/log.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/queue.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "system/asm_specifics.h"
#include "system/processor/processor.h"
#include <mem/heap.h>

// --- inline functions ---
static ALWAYS_INLINE sched_frame_t *create_sched_frame(proc_t* proc, enum SCHED_FRAME_USAGE_LEVEL level);
static ALWAYS_INLINE void destroy_sched_frame(sched_frame_t* frame);
static ALWAYS_INLINE sched_frame_t* find_sched_frame(proc_id_t proc);
static USED thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr);
static ALWAYS_INLINE void set_previous_thread(thread_t* thread);
static ALWAYS_INLINE void set_current_proc(proc_t* proc);

/* Default scheduler tick function */
static registers_t *sched_tick(registers_t *registers_ptr);

/*!
 * @brief: Initialize a scheduler on cpu @cpu
 *
 * This also attaches the scheduler to said processor
 * and from here on out, the processor owns scheduler memory
 */
ANIVA_STATUS init_scheduler(uint32_t cpu)
{
  scheduler_t* this;
  processor_t* processor;

  /* Make sure we don't get fucked */
  disable_interrupts();

  processor = processor_get(cpu);

  /* Can't init a processor with an active scheduler */
  if (!processor || processor->m_scheduler)
    return ANIVA_FAIL;

  processor->m_scheduler = kmalloc(sizeof(*this));

  if (!processor->m_scheduler)
    return ANIVA_FAIL;

  this = processor->m_scheduler;

  memset(this, 0, sizeof(*this));

  this->lock = create_mutex(0);
  this->f_tick = sched_tick;

  init_scheduler_queue(&this->processes);

  set_current_handled_thread(nullptr);
  set_current_proc(nullptr);

  return ANIVA_SUCCESS;
}

/*!
 * @brief: Quick helper to get the scheduler of a processor
 */
scheduler_t* get_scheduler_for(uint32_t cpu)
{
  processor_t* p;

  p = processor_get(cpu);

  if (!p)
    return nullptr;

  return p->m_scheduler;
}

/*!
 * @brief: Starts up the scheduler on the current CPU
 *
 * Should not return really, so TODO: make this panic on failure
 */
void start_scheduler(void) 
{
  scheduler_t* sched;
  proc_t* initial_process;
  thread_t *initial_thread;
  sched_frame_t *frame_ptr;

  sched = get_current_scheduler();

  if (!sched)
    return;

  frame_ptr = scheduler_queue_peek(&sched->processes);

  if (!frame_ptr)
    return;

  initial_process = frame_ptr->m_proc_to_schedule;

  ASSERT_MSG((initial_process->m_flags & PROC_KERNEL) == PROC_KERNEL, "FATAL: initial process was not a kernel process");

  // let's jump into the idle thread initially, so we can then
  // resume to the thread-pool when we leave critical sections
  initial_thread = initial_process->m_init_thread;

  /* Set some processor state */
  set_current_proc(frame_ptr->m_proc_to_schedule);
  set_current_handled_thread(initial_thread);
  set_previous_thread(nullptr);

  sched->flags |= SCHEDULER_FLAG_RUNNING;
  g_system_info.sys_flags |= SYSFLAGS_HAS_MULTITHREADING;

  bootstrap_thread_entries(initial_thread);
}

/*!
 * @brief: Checks if we really want to schedule a process
 */
static bool scheduler_should_schedule(scheduler_t* s, sched_frame_t* frame)
{
  /* TODO: */
  return true;
}

/*!
 * @brief: Pick the next process to put in the front of the schedule queue
 *
 * This requeues the current first process and cycles untill it finds one it likes
 */
static void pick_next_process_scheduler() 
{
  scheduler_t* s;
  sched_frame_t* current;

  s = get_current_scheduler();

  if (!s)
    return;

  /* Find the first process in the queue */
  current = scheduler_queue_peek(&s->processes);

  while (current) {
    scheduler_queue_requeue(&s->processes, current);

    current = scheduler_queue_peek(&s->processes);

    /* Valid? */
    if(!proc_can_schedule(current->m_proc_to_schedule))
      continue;

    /* Do we like this process? */
    if (scheduler_should_schedule(s, current))
      break;
  }
}

/*
 * Try to fetch another thread for us to schedule.
 * If this current process is in idle mode, we just 
 * schedule the idle thread
 */
ErrorOrPtr pick_next_thread_scheduler(void) 
{
  scheduler_t* s;
  sched_frame_t* frame;
  thread_t *next_thread;

  s = get_current_scheduler();

  if (!s)
    return Error();

  frame = scheduler_queue_peek(&s->processes);

  if (!frame)
    return Error();

  /* Check if this frame can be scheduled */
  if (!proc_can_schedule(frame->m_proc_to_schedule))
    pick_next_process_scheduler();

  /* Reset frame */
  frame = scheduler_queue_peek(&s->processes);

  set_current_proc(frame->m_proc_to_schedule);

  // we should now either be in the kernel boot context, or in this mfs context
  thread_t *prev_thread = get_current_scheduling_thread();

  if (frame->m_proc_to_schedule->m_flags & PROC_IDLE) {
    kernel_panic("FIXME: Trying to set idlethread");

    println(prev_thread->m_name);
    //set_previous_thread(frame->m_proc_to_schedule->m_idle_thread);
    set_current_handled_thread(frame->m_proc_to_schedule->m_idle_thread);

    return Success(0);
  }

  next_thread = pull_runnable_thread_sched_frame(frame);

  /* Cycle until we find shit */
  while (!next_thread) {
    pick_next_process_scheduler();

    frame = scheduler_queue_peek(&s->processes);

    next_thread = pull_runnable_thread_sched_frame(frame);
  }

  set_previous_thread(prev_thread);
  set_current_handled_thread(next_thread);

  return Success(0);
}

/*!
 * @brief: Helper to easily track interrupt disables and disable
 */
static inline void scheduler_try_disable_interrupts(scheduler_t* s)
{
  if (!interrupts_are_enabled() && !s->interrupt_depth)
    s->interrupt_depth++;

  disable_interrupts();

  if (!s)
    return;

  s->interrupt_depth++;
}

/*!
 * @brief: Helper to easily track interrupt enables and enable
 */
static inline void scheduler_try_enable_interrupts(scheduler_t* s)
{

  if (!s)
    return;

  if (s->interrupt_depth)
    s->interrupt_depth--;

  if (!s->interrupt_depth)
    enable_interrupts();
}

/*!
 * @brief: Pause the scheduler from scheduling
 *
 * This disables interrupts for a moment to make sure we don't get 
 * interrupted right before we actually pause the scheduler
 *
 * When we're already paused, we just increment s_sched_pause_depth and
 * return
 *
 * Pausing the scheduler means that someone/something wants to edit the scheduler
 * context (adding/removing processes, threads, ect.) and for that it needs to know 
 * for sure the scheduler does not decide to suddenly remove it from existence
 */
ANIVA_STATUS pause_scheduler() 
{
  scheduler_t* s;

  s = get_current_scheduler();

  if (!s)
    return ANIVA_FAIL;

  scheduler_try_disable_interrupts(s);

  /* If we are trying to pause inside of a pause, just increment the pause depth */
  if (s->pause_depth)
    goto skip_mutex;

  mutex_lock(s->lock);

  s->flags |= SCHEDULER_FLAG_PAUSED;

skip_mutex:
  s->pause_depth++;

  scheduler_try_enable_interrupts(s);
  return ANIVA_SUCCESS;
}

/*!
 * @brief: Unpause the scheduler from scheduling
 *
 * This disables interrupts for a moment to make sure we don't get 
 * interrupted right before we actually resume the scheduler
 *
 * Only sets the scheduler back to SCHEDULING once we've reached a 
 * s_sched_pause_depth of 0
 */
ANIVA_STATUS resume_scheduler()
{
  scheduler_t* s;

  s = get_current_scheduler();

  if (!s)
    return ANIVA_FAIL;

  /* Simply not paused /-/ */
  if (!s->pause_depth)
    return ANIVA_FAIL;

  scheduler_try_disable_interrupts(s);

  /* Only decrement if we can */
  if (s->pause_depth)
    s->pause_depth--;

  /* Only unlock if we just decremented to the last pause level =) */
  if (!s->pause_depth) {
    // make sure the scheduler is in the right state
    s->flags &= ~SCHEDULER_FLAG_PAUSED;

    /* Unlock the mutex */
    mutex_unlock(s->lock);
  }

  scheduler_try_enable_interrupts(s);
  return ANIVA_SUCCESS;
}

static inline bool scheduler_has_request(scheduler_t* s)
{
  return (s->flags & SCHEDULER_FLAG_HAS_REQUEST) == SCHEDULER_FLAG_HAS_REQUEST;
}

static inline void scheduler_clear_request(scheduler_t* s)
{
  s->flags &= ~SCHEDULER_FLAG_HAS_REQUEST;
}

/*!
 * @brief: Notify the scheduler that we want to reschedule
 */
void scheduler_set_request(scheduler_t* s)
{
  s->flags |= SCHEDULER_FLAG_HAS_REQUEST;
}

void scheduler_yield() 
{
  scheduler_t* s;
  processor_t *current;
  thread_t *current_thread;

  s = get_current_scheduler();

  ASSERT_MSG(s, "Tried to yield without a scheduler!");
  ASSERT_MSG((s->flags & SCHEDULER_FLAG_RUNNING) == SCHEDULER_FLAG_RUNNING, "Tried to yield to an unstarted scheduler!");
  ASSERT_MSG(!s->pause_depth, "Tried to yield to a paused scheduler!");

  CHECK_AND_DO_DISABLE_INTERRUPTS();

  // invoke the scheduler early
  scheduler_set_request(s);

  // prepare the next thread
  if (IsError(pick_next_thread_scheduler()))
    kernel_panic("TODO: terminate the process which has no threads left!");

  current = get_current_processor();
  current_thread = get_current_scheduling_thread();

  ASSERT_MSG(current_thread != nullptr, "trying to yield the scheduler while not having a current scheduling thread!");

  // in this case we don't have to wait for us to exit a
  // critical CPU section, since we are not being interrupted at all
  if (current->m_irq_depth == 0 && atomic_ptr_read(current->m_critical_depth) == 0)
    scheduler_try_execute();

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
}

/*!
 * @brief: Try to execute a schedule on the current scheduler
 *
 * NOTE: this requires the scheduler to be paused
 */
int scheduler_try_execute() 
{
  processor_t *p;
  scheduler_t* s;

  p = get_current_processor();
  s = get_current_scheduler();

  ASSERT_MSG(p && s, "Could not get current processor while trying to calling scheduler");
  ASSERT_MSG(p->m_irq_depth == 0, "Trying to call scheduler while in irq");

  if (atomic_ptr_read(p->m_critical_depth))
    return -1;

  /* If we don't want to schedule, or our scheduler is paused, return */
  if (!scheduler_has_request(s))
    return -1;

  scheduler_clear_request(s);

  thread_t *next_thread = get_current_scheduling_thread();
  thread_t *prev_thread = get_previous_scheduled_thread();

  thread_switch_context(prev_thread, next_thread);

  return 0;
}

/*!
 * @brief: The beating heart of the scheduler
 * 
 * TODO: refactor
 */
static registers_t *sched_tick(registers_t *registers_ptr) 
{
  scheduler_t* this;
  sched_frame_t *current_frame;
  thread_t *current_thread;

  this = get_current_scheduler();

  if (!this)
    return registers_ptr;

  if ((this->flags & SCHEDULER_FLAG_RUNNING) != SCHEDULER_FLAG_RUNNING)
    return registers_ptr;

  if (mutex_is_locked(this->lock) || this->pause_depth)
    return registers_ptr;

  if (!get_current_scheduling_thread())
    return registers_ptr;

  current_frame = scheduler_queue_peek(&this->processes);

  if (!proc_can_schedule(current_frame->m_proc_to_schedule)) {
    Must(pick_next_thread_scheduler());

    scheduler_set_request(this);
    return registers_ptr;
  }

  if (this->processes.count > 1) {

    // FIXME: switch timings seem to be wonky
    if (current_frame->m_sched_ticks_left <= 0) {
      current_frame->m_sched_ticks_left = current_frame->m_max_ticks;

      // switch_frames
      //println("Should have switched frames!");

      /* Pick a next process */
      pick_next_process_scheduler();

      /* Pick a next thread */
      Must(pick_next_thread_scheduler());

      /* Debug */
      //println(get_current_proc()->m_name);
      //println(get_current_scheduling_thread()->m_name);
      //println(to_string((uintptr_t)get_current_scheduling_thread()->f_entry));

      scheduler_set_request(this);
      return registers_ptr;
    }
    current_frame->m_sched_ticks_left--;
  }

  current_frame->m_proc_to_schedule->m_ticks_used++;

  current_thread = get_current_scheduling_thread();
  ASSERT_MSG(current_thread, "No initial thread!");

  current_thread->m_ticks_elapsed++;

  if (current_thread->m_ticks_elapsed >= current_thread->m_max_ticks) {
    current_thread->m_ticks_elapsed = 0;
    // we want to schedule a new thread at this point
    if (IsError(pick_next_thread_scheduler())) {
      // TODO: we might try to spawn another eternal process
      kernel_panic("Could not pick another thread to schedule!");
    }
    scheduler_set_request(this);
  }

  //resume_scheduler();
  return registers_ptr;
}

/*!
 * @brief: Add a process to the current scheduler queue
 */
ANIVA_STATUS sched_add_proc(proc_t *proc) 
{
  scheduler_t* s;
  sched_frame_t* frame;

  if (!proc)
    return ANIVA_FAIL;

  s = get_current_scheduler();

  if (!s)
    return ANIVA_FAIL;

  /* NOTE: this only fails if we try to pause the scheduler before it has been started */
  pause_scheduler();

  frame = create_sched_frame(proc, LOW);

  scheduler_queue_enqueue(&s->processes, frame);

  resume_scheduler();
  return ANIVA_SUCCESS;
}

ErrorOrPtr sched_add_priority_proc(proc_t* proc, bool reschedule) 
{
  scheduler_t* s;
  sched_frame_t* frame;

  s = get_current_scheduler();

  if (!s)
    return Error();

  pause_scheduler();

  frame = create_sched_frame(proc, LOW);

  scheduler_queue_enqueue_behind(&s->processes, s->processes.dequeue, frame);

  if (reschedule) {
    frame = scheduler_queue_peek(&s->processes);

    // Drain time left, so it gets rescheduled on the next tick
    frame->m_sched_ticks_left = 0;

    /* We'll have to unlock the mutex here also, to avoid nasty shit */
    resume_scheduler();

    /* Yield to the scheduler to initialize the reschedule */
    scheduler_yield();

    return Success(0);
  }

  resume_scheduler();
  return Success(0);
}

ANIVA_STATUS sched_remove_proc(proc_t *proc) 
{
  /* Should never remove the kernel process lmao */
  if (!proc || is_kernel(proc))
    return ANIVA_FAIL;

  return sched_remove_proc_by_id(proc->m_id);
}

/*
 * We require the scheduler to be locked by this 
 * point, which generaly means it is paused
 */
ANIVA_STATUS sched_remove_proc_by_id(proc_id_t id) 
{
  ANIVA_STATUS res;
  scheduler_t* s;
  sched_frame_t* frame;

  s = get_current_scheduler();

  if (!s)
    return ANIVA_FAIL;

  frame = find_sched_frame(id);

  if (!frame)
    return ANIVA_FAIL;

  pause_scheduler();

  /* Just hard force this bitch */
  res = scheduler_queue_remove(&s->processes, frame);

  /* Make sure we release the frames memory */
  destroy_sched_frame(frame);

  resume_scheduler();
  return res;
}

/*!
 * @brief: Allocate memory for a scheduler frame
 */
ALWAYS_INLINE sched_frame_t *create_sched_frame(proc_t* proc, enum SCHED_FRAME_USAGE_LEVEL level) 
{
  sched_frame_t *ptr;

  ptr = kmalloc(sizeof(sched_frame_t));

  if (!ptr)
    return nullptr;

  memset(ptr, 0, sizeof(*ptr));

  ptr->m_fram_usage_lvl = level;
  ptr->m_scheduled_thread_index = 0;
  ptr->m_proc_to_schedule = proc;
  ptr->m_sched_ticks_left = SCHED_FRAME_DEFAULT_START_TICKS;
  ptr->m_max_ticks = SCHED_FRAME_DEFAULT_START_TICKS;

  return ptr;
}

/*!
 * @brief: Deallocate scheduler frame memory
 */
static ALWAYS_INLINE void destroy_sched_frame(sched_frame_t* frame)
{
  kfree(frame);
}

/*
 * We just linear search through the list, since it does not really hurt 
 * performance with a small amount of processes. When this number grows though,
 * we might need to look into sorting processes based on proc_id, and the 
 * preforming a binary search, so TODO
 */
static ALWAYS_INLINE sched_frame_t* find_sched_frame(proc_id_t procid) 
{
  sched_frame_t* frame;
  scheduler_t* s;

  s = get_current_scheduler();

  if (!s)
    return nullptr;

  for (frame = s->processes.dequeue; frame; frame = frame->previous) {

    if (frame->m_proc_to_schedule->m_id == procid)
      break;
  }

  return frame;
}

#define SCHED_MAX_PULL_CYCLES 3

/*
 * TODO: redo this function, as it is way too messy and
 * should be as optimal as possible
 * perhaps we can use a queue or something (it's unclear to me which ds works best here...)
 *
 * Returning nullptr means that this thread does not have any threads left. Let's just kill it at that point...
 */
thread_t *pull_runnable_thread_sched_frame(sched_frame_t* ptr) 
{
  uint32_t prev_sched_thread_idx = ptr->m_scheduled_thread_index;
  uintptr_t cycles = 0;
  bool found;
  uintptr_t current_idx = prev_sched_thread_idx + 1;
  thread_t* next_thread = nullptr;
  proc_t* proc = ptr->m_proc_to_schedule;
  list_t *thread_list_ptr = proc->m_threads;

  if (thread_list_ptr->m_length == 0)
    return nullptr;

  if (current_idx >= thread_list_ptr->m_length)
    current_idx = 0;

  /* Clear the flag and run initializer */
  if ((proc->m_flags & PROC_UNRUNNED) == PROC_UNRUNNED) {
    proc->m_flags &= ~PROC_UNRUNNED;

    /* We require the initial thread to be in the threads list */
    ptr->m_scheduled_thread_index = 0;

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

    /* Only thread of this process isn't runnable, just fuck off and go next lmao */
    return nullptr;
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

    found = false;

    switch (next_thread->m_current_state) {
      case INVALID:
      case NO_CONTEXT:
      case STOPPED:
        // TODO: let's figure out how to handle blocked threads (so waiting on IO or some shit)
        // since we don't have any real IO yet this is kind of a placeholder lmao
      case BLOCKED:
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
        list_remove_ex(thread_list_ptr, next_thread);

        /* Let's recurse here, in order to update our loop */
        return pull_runnable_thread_sched_frame(ptr);
      default:
        break;
    }

    if (found)
      break;

    current_idx++;
  }

  ASSERT_MSG(next_thread, "FATAL: Tried to pick a null-thread to schedule!");

  // update the scheduler
  ptr->m_scheduled_thread_index = current_idx;

  return next_thread;
}

/*!
 * @brief: Sets the current thread for the current processor
 * doing this means we have two pointers that point to the
 * same memory address (thread) idk if this might get ugly
 * TODO: sync?
 */
void set_current_handled_thread(thread_t* thread) 
{
  scheduler_t* s;

  s = get_current_scheduler();

  if (!s)
    return;

  scheduler_try_disable_interrupts(s);

  processor_t *current_processor = get_current_processor();
  current_processor->m_current_thread = thread;

  scheduler_try_enable_interrupts(s);
}

/*!
 * @brief: Sets the current proc for the current processor
 */
static ALWAYS_INLINE void set_current_proc(proc_t* proc) 
{
  scheduler_t* s;

  s = get_current_scheduler();

  if (!s)
    return;

  scheduler_try_disable_interrupts(s);

  processor_t *current_processor = get_current_processor();
  current_processor->m_current_proc = proc;

  scheduler_try_enable_interrupts(s);
}

ALWAYS_INLINE void set_previous_thread(thread_t* thread) 
{
  scheduler_t* s;
  sched_frame_t *frame;
  processor_t *current_processor;

  s = get_current_scheduler();

  if (!s)
    return;

  current_processor = get_current_processor();

  scheduler_try_disable_interrupts(s);

  frame = scheduler_queue_peek(&s->processes);
  frame->m_proc_to_schedule->m_prev_thread = thread;
  current_processor->m_previous_thread = thread;

  scheduler_try_enable_interrupts(s);
}

thread_t *get_current_scheduling_thread() 
{
  return (thread_t*)read_gs(GET_OFFSET(processor_t , m_current_thread));
}

thread_t *get_previous_scheduled_thread() 
{
  return (thread_t*)read_gs(GET_OFFSET(processor_t , m_previous_thread));
}

proc_t* get_current_proc() 
{
  return (proc_t*)read_gs(GET_OFFSET(processor_t , m_current_proc));
}

proc_t* sched_get_kernel_proc() 
{
  return (proc_t*)read_gs(GET_OFFSET(processor_t, m_kernel_process));
}

scheduler_t* get_current_scheduler()
{
  return (scheduler_t*)read_gs(GET_OFFSET(processor_t, m_scheduler));
}

void set_kernel_proc(proc_t* proc) 
{
  processor_t* current = get_current_processor();
  if (current->m_kernel_process != nullptr) {
    return;
  }
  current->m_kernel_process = proc;
}

#ifndef __ANIVA_PROCESSOR__
#define __ANIVA_PROCESSOR__

#include <libk/stddef.h>
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/PagingComplex.h"
#include "proc/thread.h"
#include "processor_info.h"
#include "gdt.h"
#include "sync/atomic_ptr.h"
#include "sync/spinlock.h"
#include "system/processor/fpu/state.h"
#include "sched/scheduler.h"
#include "dev/debug/serial.h"
#include <aniva/system/asm_specifics.h>
#include <interupts/interupts.h>

struct Processor;

typedef void (*PROCESSOR_LATE_INIT)(
  struct Processor *this
);

typedef struct Processor {
  struct Processor *m_own_ptr;
  gdt_pointer_t m_gdtr;
  gdt_entry_t m_gdt[32];
  tss_entry_t m_tss;

  size_t m_gdt_highest_entry;

  //spinlock_t *m_hard_processor_lock;
  atomic_ptr_t *m_locked_level;

  atomic_ptr_t *m_critical_depth;

  uint32_t m_irq_depth;
  uint32_t m_prev_irq_depth;
  // 0 means this is the bsp
  uint32_t m_cpu_num;

  void *m_user_stack;

  PagingComplex_t* m_page_dir;
  // TODO: cpu info (features, bitwidth, vendorID, ect.)
  struct processor_info m_info;

  // TODO: threading
  thread_t *m_current_thread;
  thread_t *m_previous_thread;
  thread_t *m_root_thread;
  list_t *m_processes;

  proc_t *m_kernel_process;
  bool m_being_handled_by_scheduler;

  PROCESSOR_LATE_INIT fLateInit;
} Processor_t;

extern Processor_t g_bsp;

/*
 * initialize early aspects of the processor abstraction
 * i.e. interrupts or fields
 */
ANIVA_STATUS init_processor(Processor_t *processor, uint32_t cpu_num);

/*
 * create processor on heap
 */
Processor_t *create_processor(uint32_t num);

/*
 * create a GDT for this processor
 */
ANIVA_STATUS init_gdt(Processor_t *processor);

/*
 * check if this is the processor we used to boot
 */
bool is_bsp(Processor_t *processor);

/*
 * set the ptr to processor we used to boot
 */
void set_bsp(Processor_t *processor);

/*
 * update gdt registers n stuff
 */
void flush_gdt(Processor_t *processor);

/*
 * called when we enter an interrupt or something as such
 */
extern void processor_enter_interruption(registers_t* registers, bool irq);

/*
 * called when we exit an interrupt or something as such
 */
extern void processor_exit_interruption(registers_t* registers);

ALWAYS_INLINE Processor_t *get_current_processor() {
  return (Processor_t *) read_gs(GET_OFFSET(Processor_t, m_own_ptr));
}

ALWAYS_INLINE void processor_increment_critical_depth(Processor_t *processor) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  uintptr_t current_level = atomic_ptr_load(processor->m_critical_depth);
  atomic_ptr_write(processor->m_critical_depth, current_level + 1);
  CHECK_AND_TRY_ENABLE_INTERRUPTS();
}

ALWAYS_INLINE void processor_decrement_critical_depth(Processor_t *processor) {
  uintptr_t current_level = atomic_ptr_load(processor->m_critical_depth);

  ASSERT_MSG(current_level > 0, "Tried to leave a critical processor section while not being in one!");

  if (current_level == 1) {

    if (processor->m_irq_depth == 0) {
      // TODO: call the deferred_call_event here
      // we should make sure that we dont enter any
      // critical sections here ;-;

      scheduler_try_execute();
    }
  }

  atomic_ptr_write(processor->m_critical_depth, current_level - 1);
}

static ALWAYS_INLINE uint64_t get_user_stack_offset() {
  return GET_OFFSET(Processor_t, m_user_stack);
}

static ALWAYS_INLINE uint64_t get_kernel_stack_offset() {
  return GET_OFFSET(Processor_t, m_tss) + GET_OFFSET(tss_entry_t , rsp0l);
}

extern FpuState standard_fpu_state;

#endif // !__ANIVA_PROCESSOR__

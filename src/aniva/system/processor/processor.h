#ifndef __ANIVA_PROCESSOR__
#define __ANIVA_PROCESSOR__

#include <libk/stddef.h>
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/data/queue.h"
#include "mem/pg.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "processor_info.h"
#include "gdt.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/processor/fpu/state.h"
#include "sched/scheduler.h"
#include "dev/debug/serial.h"
#include <aniva/system/asm_specifics.h>
#include <intr/interrupts.h>

struct processor;
struct dev_manifest;

#define PROCESSOR_FLAG_INT_SYSCALLS     (0x00000001) /* Do we use interrupts for syscalls, or does this CPU have the SYSCALL feature (sysenter/sysexit) */
#define PROCESSOR_FLAG_XSAVE            (0x00000002) /* Do we have xsave for fpu stuff */
#define PROCESSOR_FLAG_VIRTUAL          (0x00000004) /* Is this a virtual (emulated) or a physical processor */

typedef struct processor {
  struct processor *m_own_ptr;

  /* GDT */
  gdt_entry_t m_gdt[7]              __attribute__((aligned(0x10)));
  tss_entry_t m_tss                 __attribute__((aligned(0x10)));
  gdt_pointer_t m_gdtr              __attribute__((aligned(0x10)));

  //spinlock_t *m_hard_processor_lock;
  atomic_ptr_t *m_locked_level;
  atomic_ptr_t *m_critical_depth;

  scheduler_t* m_scheduler;

  uint32_t m_irq_depth;
  uint32_t m_prev_irq_depth;
  // 0 means this is the bsp
  uint32_t m_cpu_num;
  uint32_t m_flags;

  void *m_user_stack;

  pml_entry_t* m_page_dir;
  // TODO: cpu info (features, bitwidth, vendorID, ect.)
  struct processor_info m_info;

  // TODO: threading
  thread_t *m_current_thread;
  thread_t *m_previous_thread;
  thread_t *m_root_thread;
  list_t *m_processes;

  proc_t *m_current_proc;
  proc_t *m_kernel_process;
} processor_t;

extern processor_t g_bsp;

/*
 * initialize early aspects of the processor abstraction
 * i.e. interrupts or fields
 */
void init_processor(processor_t *processor, uint32_t cpu_num);
void init_processor_late(processor_t* processor);

processor_t *create_processor(uint32_t num);

ANIVA_STATUS init_gdt(processor_t *processor);

processor_t* processor_get(uint32_t cpu_id);
/* TODO: implement */
int processor_sleep(processor_t* processor);
/* TODO: implement */
int processor_wake(processor_t* processor);

bool is_bsp(processor_t *processor);
void set_bsp(processor_t *processor);

void flush_gdt(processor_t *processor);

extern void processor_enter_interruption(registers_t* registers, bool irq);
extern void processor_exit_interruption(registers_t* registers);

ALWAYS_INLINE processor_t *get_current_processor() {
  return (processor_t *) read_gs(GET_OFFSET(processor_t, m_own_ptr));
}

ALWAYS_INLINE void processor_increment_critical_depth(processor_t *processor) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  uintptr_t current_level = atomic_ptr_load(processor->m_critical_depth);
  atomic_ptr_write(processor->m_critical_depth, current_level + 1);
  CHECK_AND_TRY_ENABLE_INTERRUPTS();
}

ALWAYS_INLINE void processor_decrement_critical_depth(processor_t *processor) {
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
  return GET_OFFSET(processor_t, m_user_stack);
}

static ALWAYS_INLINE uint64_t get_kernel_stack_offset() {
  return GET_OFFSET(processor_t, m_tss) + GET_OFFSET(tss_entry_t , rsp0l);
}

extern FpuState standard_fpu_state;

#endif // !__ANIVA_PROCESSOR__

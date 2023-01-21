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
#include <aniva/system/asm_specifics.h>

#define PROCESSOR_MAX_GDT_ENTRIES 7
#define MSR_FS_BASE 0xc0000100
#define MSR_GS_BASE 0xc0000101

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

  spinlock_t *m_hard_processor_lock;
  atomic_ptr_t *m_locked_level;
  atomic_ptr_t *m_vital_task_depth;

  uint32_t m_irq_depth;
  uint32_t m_cpu_num;

  void *m_stack_start;

  PagingComplex_t* m_page_dir;
  // TODO: cpu info (features, bitwidth, vendorID, ect.)
  struct ProcessorInfo m_info;

  // TODO: threading
  thread_t *m_current_thread;
  thread_t *m_root_thread;
  list_t *m_processes;
  bool m_being_handled_by_scheduler;

  PROCESSOR_LATE_INIT fLateInit;
} Processor_t;

ProcessorInfo_t processor_gather_info();

ANIVA_STATUS init_processor(Processor_t *processor, uint32_t cpu_num);

Processor_t *create_processor(uint32_t num);

ANIVA_STATUS init_gdt(Processor_t *processor);

bool is_bsp(Processor_t *processor);

void set_bsp(Processor_t *processor);

void flush_gdt(Processor_t *processor);

ANIVA_STATUS init_processor_ctx(Processor_t *processor, thread_t *);

ANIVA_STATUS init_processor_dynamic_ctx(Processor_t *processor, thread_t *);

ALWAYS_INLINE Processor_t *get_current_processor() {
  return (Processor_t *) read_gs(GET_OFFSET(Processor_t, m_own_ptr));
}

ALWAYS_INLINE void processor_increment_vital_depth(Processor_t *processor) {
  uintptr_t current_level = atomic_ptr_load(processor->m_vital_task_depth);
  atomic_ptr_write(processor->m_vital_task_depth, current_level + 1);
}

ALWAYS_INLINE void processor_decrement_vital_depth(Processor_t *processor) {
  uintptr_t current_level = atomic_ptr_load(processor->m_vital_task_depth);
  if (current_level > 0) {
    atomic_ptr_write(processor->m_vital_task_depth, current_level - 1);
  }
}

extern FpuState standard_fpu_state;

#endif // !__ANIVA_PROCESSOR__

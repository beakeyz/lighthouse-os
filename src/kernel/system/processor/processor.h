#ifndef __LIGHT_PROCESSOR__
#define __LIGHT_PROCESSOR__
#include <libk/stddef.h>
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/PagingComplex.h"
#include "proc/thread.h"
#include "processor_info.h"
#include "gdt.h"
#include "system/processor/fpu/state.h"

#define PROCESSOR_MAX_GDT_ENTRIES 7
#define MSR_FS_BASE 0xc0000100
#define MSR_GS_BASE 0xc0000101

struct Processor;

typedef void (*PROCESSOR_LATE_INIT) (
  struct Processor* this
);

typedef struct Processor {
  gdt_pointer_t m_gdtr;
  gdt_entry_t m_gdt[PROCESSOR_MAX_GDT_ENTRIES];
  tss_entry_t m_tss;

  size_t m_gdt_highest_entry;

  uint32_t m_irq_depth;
  uint32_t m_cpu_num;

  void* m_stack_start;

  PagingComplex_t m_page_dir;
  
  // TODO: cpu info (features, bitwidth, vendorID, ect.)
  struct ProcessorInfo m_info;

  // TODO: threading
  thread_t* m_current_thread;
  thread_t* m_root_thread;
  list_t* m_processes;

  PROCESSOR_LATE_INIT fLateInit;
} Processor_t;

ProcessorInfo_t processor_gather_info();
LIGHT_STATUS init_processor(Processor_t* processor, uint32_t cpu_num);

LIGHT_STATUS init_gdt(Processor_t* processor);

bool is_bsp(Processor_t* processor);
void set_bsp(Processor_t* processor);

void flush_gdt(Processor_t* processor);

LIGHT_STATUS init_processor_ctx(Processor_t* processor, thread_t*);
LIGHT_STATUS init_processor_dynamic_ctx(Processor_t* processor, thread_t*);

extern FpuState standard_fpu_state;

#endif // !__LIGHT_PROCESSOR__

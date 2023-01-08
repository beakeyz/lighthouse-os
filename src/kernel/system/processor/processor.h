#ifndef __LIGHT_PROCESSOR__
#define __LIGHT_PROCESSOR__
#include <libk/stddef.h>
#include "libk/error.h"
#include "proc/thread.h"
#include "processor_info.h"
#include "gdt.h"

struct Processor;

// TODO: move to fpu/state.h?
typedef struct {
  // lr
  // header
  // ext save area (CPUID)
} __attribute__((aligned(64), packed)) FpuState;

typedef void (*PROCESSOR_LATE_INIT) (
  struct Processor* this
);

typedef struct Processor {
  gdt_pointer_t m_gdtr;
  gdt_entry_t m_gdt[256];
  tss_entry_t m_tss;

  uint32_t m_irq_depth;
  uint32_t m_cpu_num;

  void* m_stack_start;
  
  // TODO: cpu info (features, bitwidth, vendorID, ect.)
  struct ProcessorInfo m_info;

  // TODO: threading
  thread_t* m_current_thread;

  thread_t* m_root_thread;

  PROCESSOR_LATE_INIT fLateInit;
} Processor_t;

ProcessorInfo_t processor_gather_info();
Processor_t init_processor(uint32_t cpu_num);

LIGHT_STATUS init_gdt(Processor_t* processor);

bool is_bsp(Processor_t* processor);
void set_bsp(Processor_t* processor);

#endif // !__LIGHT_PROCESSOR__

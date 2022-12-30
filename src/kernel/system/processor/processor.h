#ifndef __LIGHT_PROCESSOR__
#define __LIGHT_PROCESSOR__
#include <libk/stddef.h>
#include "interupts/gdt.h"

struct Processor;
struct ProcessorInfo;

typedef struct ProcessorInfo {
  // TODO:
} ProcessorInfo_t;

typedef void (*PROCESSOR_LATE_INIT) (
  struct Processor* this
);

typedef struct Processor {
  gdt_pointer_t m_gdtr;
  gdt_entry_t m_gdt[256];
  tss_entry_t m_tss;

  uint32_t m_cpu_num;

  void* m_stack_start;
  
  // TODO: cpu info (features, bitwidth, ect.)
  struct ProcessorInfo m_info;
  // TODO: threading
  PROCESSOR_LATE_INIT fLateInit;
} Processor_t;

ProcessorInfo_t processor_gather_info();
Processor_t init_processor(uint32_t cpu_num);

bool is_bsp(Processor_t* processor);

#endif // !__LIGHT_PROCESSOR__

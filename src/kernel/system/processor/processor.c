#include "processor.h"
#include "dev/debug/serial.h"
#include "interupts/idt.h"
#include "kmain.h"
#include "libk/stddef.h"
#include <interupts/interupts.h>

static ALWAYS_INLINE void processor_late_init(Processor_t* this) __attribute__((used));
static ALWAYS_INLINE void write_to_gdt(uint16_t selector, gdt_entry_t* entry);

ProcessorInfo_t processor_gather_info() {
  ProcessorInfo_t ret = {};
  // TODO:
  return ret;
}
Processor_t init_processor(uint32_t cpu_num) {
  Processor_t ret = {0};

  ret.m_cpu_num = cpu_num;
  ret.m_irq_depth = 0;

  ret.fLateInit = processor_late_init;

  // TODO:
  init_gdt(&ret);

  return ret;
}

static ALWAYS_INLINE void processor_late_init(Processor_t* this) {
  // TODO: 
  if (is_bsp(this)) {
    g_GlobalSystemInfo.m_current_core = this;
    init_int_control_management();
    init_interupts();
  } else {
    flush_idt();
  }

}

LIGHT_STATUS init_gdt(Processor_t* processor) {

  processor->m_gdtr.limit = 0;
  processor->m_gdtr.base = NULL;



  return LIGHT_SUCCESS;
}

bool is_bsp(Processor_t* processor) {
  return (processor->m_cpu_num == 0);
}

void set_bsp(Processor_t* processor) {
  if (processor != nullptr) {
    g_GlobalSystemInfo.m_current_core = processor;
  } else {
    println("[[WARNING]] null passed to set_bsp");
  }
  // FIXME: uhm, what to do when this passess null?
}

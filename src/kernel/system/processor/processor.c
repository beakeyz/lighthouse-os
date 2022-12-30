#include "processor.h"
#include "kmain.h"
#include <interupts/interupts.h>

static void processor_late_init(Processor_t* this) __attribute__((used));

ProcessorInfo_t processor_gather_info() {
  ProcessorInfo_t ret = {};
  // TODO:
  return ret;
}
Processor_t init_processor(uint32_t cpu_num) {
  Processor_t ret = {0};

  ret.m_cpu_num = cpu_num;
  ret.fLateInit = processor_late_init;

  // TODO:

  return ret;
}

static void processor_late_init(Processor_t* this) {
  // TODO: 
  if (is_bsp(this)) {
    g_GlobalSystemInfo.m_current_core = this;
  }

  init_int_control_management();
  init_interupts();

}

bool is_bsp(Processor_t* processor) {
  return (processor->m_cpu_num == 0);
}

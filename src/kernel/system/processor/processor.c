#include "processor.h"
#include <interupts/interupts.h>

static void processor_late_init(Processor_t* this) __attribute__((used));

ProcessorInfo_t processor_gather_info() {
  ProcessorInfo_t ret = {};

  return ret;
}
Processor_t init_processor(uint32_t cpu_num) {
  Processor_t ret = {0};

  ret.m_cpu_num = cpu_num;
  ret.fLateInit = processor_late_init;

  return ret;
}

static void processor_late_init(Processor_t* this) {
  // TODO: 

  init_int_control_management();
  init_interupts();

}

bool is_bsp(Processor_t* processor) {
  return (processor->m_cpu_num == 0);
}

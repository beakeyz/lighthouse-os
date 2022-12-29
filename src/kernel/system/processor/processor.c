#include "processor.h"

ProcessorInfo_t processor_gather_info() {
  ProcessorInfo_t ret = {};

  return ret;
}
Processor_t init_processor(uint32_t cpu_num) {
  Processor_t ret = {0};

  ret.m_cpu_num = cpu_num;


  return ret;
}

bool is_bsp(Processor_t* processor) {
  return (processor->m_cpu_num == 0);
}

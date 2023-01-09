#include "processor.h"
#include "dev/debug/serial.h"
#include "interupts/idt.h"
#include "kmain.h"
#include "libk/stddef.h"
#include <libk/string.h>
#include "system/processor/gdt.h"
#include <interupts/interupts.h>

static ALWAYS_INLINE void processor_late_init(Processor_t* this) __attribute__((used));
static ALWAYS_INLINE void write_to_gdt(Processor_t* this, uint16_t selector, gdt_entry_t* entry);

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

ALWAYS_INLINE void processor_late_init(Processor_t* this) {
  // TODO: 
  if (is_bsp(this)) {
    g_GlobalSystemInfo.m_current_core = this;
    init_int_control_management();
    init_interupts();
  } else {
    flush_idt();
  }

}

ALWAYS_INLINE void write_to_gdt(Processor_t* this, uint16_t selector, gdt_entry_t* entry) {
  const uint16_t index = (selector & 0xfffc) >> 3;
  const uint16_t gdt_length_at_index = (index + 1) * sizeof(gdt_entry_t); 
  const uint16_t current_limit = this->m_gdtr.limit;

  this->m_gdt[index].low = entry->low;
  this->m_gdt[index].high = entry->high;

  if (index >= this->m_gdt_highest_entry) {
    this->m_gdt_highest_entry = index+1;
  }
}

void flush_gdt(Processor_t* processor) {
  // base
  processor->m_gdtr.base = (uintptr_t)processor->m_gdt;
  // limit
  processor->m_gdtr.limit = (processor->m_gdt_highest_entry * sizeof(gdt_entry_t)) - 1;

  asm volatile("lgdt %0" :: "m"(processor->m_gdtr) : "memory");
  asm volatile (
    "mov $0x10, %ax\n"
		"mov %ax, %ds\n"
		"mov %ax, %es\n"
		"mov %ax, %fs\n"
		"mov %ax, %gs\n"
		"mov %ax, %ss\n");
}

LIGHT_STATUS init_gdt(Processor_t* processor) {

  memset(processor->m_gdt, 0x00, sizeof(processor->m_gdt));
  processor->m_gdtr.limit = 0;
  processor->m_gdtr.base = NULL;

  gdt_entry_t null = {0};
  gdt_entry_t ring0_code = {
    .low = 0x0000ffff,
    .high = 0x00af9a00,
  };
  gdt_entry_t ring0_data = {
    .low = 0x0000ffff,
    .high = 0x00af9200,
  };
  gdt_entry_t ring3_data = {
    .low = 0x0000ffff,
    .high = 0x008ff200,
  };
  gdt_entry_t ring3_code = {
    .low = 0x0000ffff,
    .high = 0x00affa00,
  };
  write_to_gdt(processor, 0x0000, &null);
  write_to_gdt(processor, GDT_KERNEL_CODE, &ring0_code);
  write_to_gdt(processor, GDT_KERNEL_DATA, &ring0_data);
  write_to_gdt(processor, GDT_USER_DATA, &ring3_data);
  write_to_gdt(processor, GDT_USER_CODE, &ring3_code);

  gdt_entry_t tss = {0};
  set_gdte_base(&tss, (uintptr_t)&processor->m_tss & 0xffffffff);
  set_gdte_limit(&tss, sizeof(tss_entry_t) - 1);

  tss.structured.dpl = 0;
  tss.structured.segment_present = 1;
  tss.structured.granularity = 0;
  tss.structured.op_size64 = 0;
  tss.structured.op_size32 = 1;
  tss.structured.descriptor_type = 0;
  tss.structured.type = AVAILABLE_TSS;
  //write_to_gdt(processor, GDT_TSS_SEL, &tss);

  gdt_entry_t tss_2 = {0};
  tss_2.low = (uintptr_t)&processor->m_tss >> 32;
  //write_to_gdt(processor, GDT_TSS_2_SEL, &tss_2);

  flush_gdt(processor);

  // load task regs

  // set msr base

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

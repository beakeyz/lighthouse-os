#include "processor.h"
#include "interupts/idt.h"
#include "system/asm_specifics.h"
#include "system/msr.h"
#include <mem/heap.h>
#include "libk/string.h"
#include "system/processor/fpu/init.h"
#include <system/syscall/core.h>

extern void _flush_gdt(uintptr_t gdtr);

static ALWAYS_INLINE void processor_late_init(Processor_t *this) __attribute__((used));
static ALWAYS_INLINE void write_to_gdt(Processor_t *this, uint16_t selector, gdt_entry_t entry);
static ALWAYS_INLINE void init_sse(Processor_t *processor);

static ALWAYS_INLINE void init_smep(processor_info_t* info);
static ALWAYS_INLINE void init_smap(processor_info_t* info);
static ALWAYS_INLINE void init_umip(processor_info_t* info);

extern FpuState standard_fpu_state;

Processor_t g_bsp;

// Should not be used to initialize bsp
Processor_t *create_processor(uint32_t num) {
  Processor_t * ret = kmalloc(sizeof(Processor_t));
  ret->m_cpu_num = num;
  ret->fLateInit = processor_late_init;
  return ret;
}

ANIVA_STATUS init_processor(Processor_t *processor, uint32_t cpu_num) {
  processor->m_own_ptr = processor;
  processor->m_cpu_num = cpu_num;
  processor->m_irq_depth = 0;
  processor->m_prev_irq_depth = 0;
  processor->fLateInit = processor_late_init;

  processor->m_info = gather_processor_info();

  init_sse(processor);

  init_smep(&processor->m_info);
  init_smap(&processor->m_info);
  init_umip(&processor->m_info);

  if (processor_has(&processor->m_info, X86_FEATURE_PGE)) {
    write_cr4(read_cr4() | 0x80);
  }

  if (processor_has(&processor->m_info, X86_FEATURE_FSGSBASE)) {
    write_cr4(read_cr4() & ~(0x10000));
  }

  if (processor_has(&processor->m_info, X86_FEATURE_XSAVE)) {
    write_cr4(read_cr4() | 0x40000);

    // TODO: xcr0?
    write_xcr0(0x1);

    if (processor_has(&processor->m_info, X86_FEATURE_AVX)) {
      write_xcr0(read_xcr0() | (AVX | SSE | X87));
    }
  }

  if (processor_has(&processor->m_info, X86_FEATURE_SYSCALL)) {
    // TODO: set syscall entry
  }
  // setup software
  if (is_bsp(processor)) {
    // TODO: do bsp shit
  }
  // TODO:
  init_gdt(processor);

  // set msr base
  wrmsr(MSR_GS_BASE, (uintptr_t)processor);
  return ANIVA_SUCCESS;
}

ALWAYS_INLINE void processor_late_init(Processor_t *this) {

  this->m_processes = init_list();
  this->m_critical_depth = create_atomic_ptr();
  this->m_locked_level = create_atomic_ptr();
  atomic_ptr_write(this->m_critical_depth, 0);
  atomic_ptr_write(this->m_locked_level, 0);

  // TODO:
  if (is_bsp(this)) {
    init_int_control_management();
    init_interupts();

    fpu_generic_init();

    // FIXME: other save mechs?
    save_fpu_state(&standard_fpu_state);
  } else {
    flush_idt();
  }
}

ALWAYS_INLINE void write_to_gdt(Processor_t *this, uint16_t selector, gdt_entry_t entry) {
  const uint16_t index = (selector & 0xfffc) >> 3;
  const uint16_t gdt_length_at_index = (index + 1) * sizeof(gdt_entry_t);
  const uint16_t current_limit = this->m_gdtr.limit;

  this->m_gdt[index].low = entry.low;
  this->m_gdt[index].high = entry.high;

  if (index >= this->m_gdt_highest_entry) {
    this->m_gdt_highest_entry = index + 1;
  }
}

ALWAYS_INLINE void init_sse(Processor_t *processor) {
  // check for feature
  const uintptr_t orig_cr0 = read_cr0();
  const uintptr_t orig_cr4 = read_cr4();

  write_cr0((orig_cr0 & 0xfffffffbU) | 0x02);
  write_cr4(orig_cr4 | 0x600);
}

void flush_gdt(Processor_t *processor) {
  // base
  processor->m_gdtr.base = (uintptr_t) & processor->m_gdt[0];
  // limit
  processor->m_gdtr.limit = (processor->m_gdt_highest_entry * sizeof(gdt_entry_t)) - 1;

  //asm volatile ("lgdt %0"::"m"(processor->m_gdtr) : "memory");


  _flush_gdt((uintptr_t)&processor->m_gdtr);
}

ANIVA_STATUS init_gdt(Processor_t *processor) {

  processor->m_gdtr.limit = 0;
  processor->m_gdtr.base = NULL;

  gdt_entry_t null = {
    .low = 0x00000000,
    .high = 0x00000000,
  };
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
  write_to_gdt(processor, 0x0000, null);
  write_to_gdt(processor, GDT_KERNEL_CODE, ring0_code);
  write_to_gdt(processor, GDT_KERNEL_DATA, ring0_data);
  write_to_gdt(processor, GDT_USER_DATA, ring3_data);
  write_to_gdt(processor, GDT_USER_CODE, ring3_code);

  gdt_entry_t tss = {0};
  set_gdte_base(&tss, ((uintptr_t) & processor->m_tss) & 0xffffffff);
  set_gdte_limit(&tss, sizeof(tss_entry_t) - 1);
  tss.structured.dpl = 0;
  tss.structured.segment_present = 1;
  tss.structured.granularity = 0;
  tss.structured.op_size64 = 0;
  tss.structured.op_size32 = 1;
  tss.structured.descriptor_type = 0;
  tss.structured.type = AVAILABLE_TSS;
  write_to_gdt(processor, GDT_TSS_SEL, tss);

  gdt_entry_t tss_2 = {0};
  tss_2.low = (uintptr_t) & processor->m_tss >> 32;
  write_to_gdt(processor, GDT_TSS_2_SEL, tss_2);

  flush_gdt(processor);

  // load task regs
  __ltr(GDT_TSS_SEL);

  return ANIVA_SUCCESS;
}

bool is_bsp(Processor_t *processor) {
  return (processor->m_cpu_num == 0);
}

void processor_enter_interruption(registers_t* registers, bool irq) {
  Processor_t *current = get_current_processor();
  ASSERT_MSG(current, "could not get current processor when entering interruption");

  current->m_prev_irq_depth = current->m_irq_depth;
  if (irq) {
    current->m_irq_depth++;
  }
}

void processor_exit_interruption(registers_t* registers) {
  Processor_t *current = get_current_processor();
  ASSERT_MSG(current, "could not get current processor when exiting interruption");

  current->m_irq_depth = current->m_prev_irq_depth;
  // call deferred calls here too?

  if (current->m_irq_depth == 0 && atomic_ptr_load(current->m_critical_depth) == 0) {
    scheduler_try_execute();
  }
}

static ALWAYS_INLINE void init_smep(processor_info_t* info) {
  if (processor_has(info, X86_FEATURE_SMEP)) {
    write_cr4(read_cr4() | 0x100000);
  }
}

static ALWAYS_INLINE void init_smap(processor_info_t* info) {
  if (processor_has(info, X86_FEATURE_SMAP)) {
    write_cr4(read_cr4() | 0x200000);
  }
}

static ALWAYS_INLINE void init_umip(processor_info_t* info) {

  if (!processor_has(&g_bsp.m_info, X86_FEATURE_UMIP) || !processor_has(info, X86_FEATURE_UMIP)) {
    write_cr4(read_cr4() & ~(0x800));
    return;
  }

  write_cr4(read_cr4() | 0x800);
}

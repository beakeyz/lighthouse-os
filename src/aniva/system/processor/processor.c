#include "processor.h"
#include "entry/entry.h"
#include "irq/ctl/irqchip.h"
#include "irq/idt.h"
#include "libk/flow/error.h"
#include "sched/scheduler.h"
#include "system/asm_specifics.h"
#include "system/msr.h"
#include "system/processor/fpu/init.h"
#include "system/processor/processor_info.h"
#include <mem/heap.h>
#include <system/syscall/core.h>

extern void _flush_gdt(uintptr_t gdtr);

static ALWAYS_INLINE void write_to_gdt(processor_t* this, uint16_t selector, gdt_entry_t entry);
static void init_sse(processor_t* processor);

static ALWAYS_INLINE void init_smep(processor_info_t* info);
static ALWAYS_INLINE void init_smap(processor_info_t* info);
static ALWAYS_INLINE void init_umip(processor_info_t* info);

processor_t g_bsp;
extern FpuState standard_fpu_state;

static ErrorOrPtr __init_syscalls(processor_t* processor)
{
    /* We support the syscall feature */
    processor->m_flags &= ~PROCESSOR_FLAG_INT_SYSCALLS;

    /* Enable syscall extention */
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | MSR_EFER_SCE);

    /* Set the syscall segments */
    wrmsr_ex(MSR_STAR, 0, (0x13 << 16) | GDT_KERNEL_CODE);

    /* Set the syscall entry */
    wrmsr(MSR_LSTAR, (uintptr_t)sys_entry);

    /* Mask all these eflag bits so there is no leaking of kernel data to userspace */
    wrmsr(MSR_SFMASK, EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_TF | EFLAGS_IF | EFLAGS_DF | EFLAGS_OF | EFLAGS_IOPL | EFLAGS_NT | EFLAGS_RF | EFLAGS_AC | EFLAGS_ID);

    return Success(0);
}

// Should not be used to initialize bsp
processor_t* create_processor(uint32_t num)
{
    processor_t* ret = kmalloc(sizeof(processor_t));
    ret->m_cpu_num = num;

    /* TODO */
    kernel_panic("TODO: support multiple processors (smp)");

    return ret;
}

void init_processor(processor_t* processor, uint32_t cpu_num)
{
    memset(processor, 0, sizeof(*processor));

    processor->m_own_ptr = processor;
    processor->m_cpu_num = cpu_num;

    init_gdt(processor);

    // set msr base
    wrmsr(MSR_GS_BASE, (uintptr_t)processor);
}

/*!
 * @brief: Quickly register a processor to the global processor array (gpa)
 */
static bool processor_register(processor_t* p)
{
    if (p->m_cpu_num >= SYS_MAX_CPU)
        return false;

    g_system_info.processors[p->m_cpu_num] = p;

    return true;
}

/*!
 * @brief: Sets up late information about a processor
 *
 * Creates heap-dependent data structures
 * Creates interrupt managers for this processor
 * Initializes FPU/SIMD/SSE
 * Registers the processor
 */
void init_processor_late(processor_t* processor)
{
    uint64_t cr4;

    processor->m_processes = init_list();
    processor->m_critical_depth = create_atomic_ptr();
    processor->m_locked_level = create_atomic_ptr();
    processor->m_info = gather_processor_info();

    atomic_ptr_write(processor->m_critical_depth, 0);
    atomic_ptr_write(processor->m_locked_level, 0);

    init_sse(processor);

    init_smep(&processor->m_info);
    init_smap(&processor->m_info);
    init_umip(&processor->m_info);

    ASSERT_MSG(processor_has(&processor->m_info, X86_FEATURE_PAT), "Processor does not support PAT!");

    cr4 = read_cr4();

    if (processor_has(&processor->m_info, X86_FEATURE_PGE))
        cr4 |= 0x80;

    /*
    if (processor_has(&processor->m_info, X86_FEATURE_PSE))
      cr4 |= 0x10;
      */

    if (processor_has(&processor->m_info, X86_FEATURE_FSGSBASE))
        cr4 &= ~(0x10000);

    if (processor_has(&processor->m_info, X86_FEATURE_XSAVE)) {
        cr4 |= 0x40000;

        // TODO: xcr0?
        write_xcr0(0x1);
        write_cr4(cr4);

        if (processor_has(&processor->m_info, X86_FEATURE_AVX)) {
            write_xcr0(read_xcr0() | (AVX | SSE | X87));
        }
    }

    /* We are guaranteed to support interrupt syscalls */
    processor->m_flags |= PROCESSOR_FLAG_INT_SYSCALLS;

    if (processor_has(&processor->m_info, X86_FEATURE_SYSCALL)) {
        // Set syscall entry
        ASSERT_MSG(__init_syscalls(processor).m_status == ANIVA_SUCCESS, "Failed to initialize syscalls");
    }

    if (is_bsp(processor)) {
        init_irq_chips();

        fpu_generic_init();

        // FIXME: other save mechs?
        save_fpu_state(&standard_fpu_state);
    } else {
        flush_idt();
    }

    /* Register the processor */
    processor_register(processor);
}

/*!
 * @brief: Gets the processor with the @cpu_id
 */
processor_t* processor_get(uint32_t cpu_id)
{
    if (cpu_id >= SYS_MAX_CPU)
        return nullptr;

    return g_system_info.processors[cpu_id];
}

ALWAYS_INLINE void write_to_gdt(processor_t* this, uint16_t selector, gdt_entry_t entry)
{
    const uint16_t index = (selector & 0xfffc) >> 3;

    this->m_gdt[index] = entry;
}

static void init_sse(processor_t* processor)
{
    // check for feature
    const uintptr_t orig_cr0 = read_cr0();
    const uintptr_t orig_cr4 = read_cr4();

    write_cr0((orig_cr0 & 0xfffffffbU) | 0x02);
    write_cr4(orig_cr4 | 0x600);
}

void flush_gdt(processor_t* processor)
{
    // base
    processor->m_gdtr.base = (uintptr_t)&processor->m_gdt[0];
    // limit
    processor->m_gdtr.limit = (sizeof(processor->m_gdt)) - 1;

    // asm volatile ("lgdt %0"::"m"(processor->m_gdtr) : "memory");

    _flush_gdt((uintptr_t)&processor->m_gdtr);
}

ANIVA_STATUS init_gdt(processor_t* processor)
{

    processor->m_gdtr.limit = 0;
    processor->m_gdtr.base = NULL;

    memset(processor->m_gdt, 0, sizeof(gdt_entry_t) * 7);
    memset(&processor->m_tss, 0, sizeof(tss_entry_t));

    gdt_entry_t null = {
        .raw = 0,
    };
    gdt_entry_t ring0_code = {
        .low = 0x0000ffff,
        .high = 0x00af9a00,
    };
    gdt_entry_t ring0_data = {
        .low = 0x0000ffff,
        .high = 0x00af9200,
    };
    gdt_entry_t ring3_code = {
        .low = 0x0000ffff,
        .high = 0x00affa00,
    };
    gdt_entry_t ring3_data = {
        .low = 0x0000ffff,
        .high = 0x00aff200,
    };

    write_to_gdt(processor, 0x00, null);
    write_to_gdt(processor, GDT_KERNEL_CODE, ring0_code);
    write_to_gdt(processor, GDT_KERNEL_DATA, ring0_data);
    write_to_gdt(processor, GDT_USER_DATA, ring3_data);
    write_to_gdt(processor, GDT_USER_CODE, ring3_code);

    uintptr_t tss_addr = (uintptr_t)&processor->m_tss;

    gdt_entry_t tss = { 0 };
    set_gdte_base(&tss, (tss_addr & 0xffffffff));
    set_gdte_limit(&tss, sizeof(tss_entry_t) - 1);
    tss.structured.dpl = 0;
    tss.structured.segment_present = 1;
    tss.structured.descriptor_type = 0;
    tss.structured.available = 0;
    tss.structured.op_size64 = 1;
    tss.structured.op_size32 = 0;
    tss.structured.granularity = 0;
    tss.structured.type = AVAILABLE_TSS;
    write_to_gdt(processor, GDT_TSS_SEL, tss);

    gdt_entry_t tss_2 = { 0 };
    tss_2.low = (tss_addr >> 32) & 0xffffffff;
    write_to_gdt(processor, GDT_TSS_2_SEL, tss_2);

    flush_gdt(processor);

    // load task regs
    __ltr(GDT_TSS_SEL);

    return ANIVA_SUCCESS;
}

bool is_bsp(processor_t* processor)
{
    return (processor->m_cpu_num == 0);
}

/*!
 * @brief Called when the processor enters an interrupted state (context switch, irq, syscall, ect.)
 *
 * The current state of the processor, registers and such get stored here
 */
void processor_enter_interruption(registers_t* registers, bool irq)
{
    processor_t* current = get_current_processor();
    ASSERT_MSG(current, "could not get current processor when entering interruption");

    if (irq)
        current->m_irq_depth++;
}

void processor_exit_interruption(registers_t* registers)
{
    processor_t* current = get_current_processor();
    ASSERT_MSG(current, "could not get current processor when exiting interruption");

    if (current->m_irq_depth > 0)
        current->m_irq_depth--;

    /* call events or deferred calls here too? */

    if (current->m_irq_depth == 0 && atomic_ptr_read(current->m_critical_depth) == 0)
        scheduler_try_execute();
}

static ALWAYS_INLINE void init_smep(processor_info_t* info)
{
    if (processor_has(info, X86_FEATURE_SMEP)) {
        write_cr4(read_cr4() | 0x100000);
    }
}

static ALWAYS_INLINE void init_smap(processor_info_t* info)
{
    if (processor_has(info, X86_FEATURE_SMAP)) {
        write_cr4(read_cr4() | 0x200000);
    }
}

static ALWAYS_INLINE void init_umip(processor_info_t* info)
{

    if (!processor_has(&g_bsp.m_info, X86_FEATURE_UMIP) || !processor_has(info, X86_FEATURE_UMIP)) {
        write_cr4(read_cr4() & ~(0x800));
        return;
    }

    write_cr4(read_cr4() | 0x800);
}

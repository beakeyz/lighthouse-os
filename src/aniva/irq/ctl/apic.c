#include "apic.h"
#include "entry/entry.h"
#include "irq/ctl/ioapic.h"
#include "irq/ctl/irqchip.h"
#include "irq/interrupts.h"
#include "libk/cmdline/parser.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem.h"
#include "system/acpi/acpi.h"
#include "system/acpi/acpica/actbl2.h"
#include "system/acpi/parser.h"
#include "system/processor/processor.h"
#include "system/processor/processor_info.h"
#include <system/acpi/tables.h>
#include <system/msr.h>

static list_t* _irq_overrides;
static lapic_t* _bsp_lapic;
static io_apic_t* _io_apic;
/* LAPIC chips on alternate processors */
static lapic_t* _ap_lapics = nullptr;
static uint32_t _ap_count = 0;

/*!
 * @brief: Add an override entry to our list
 *
 * Allocates new memory for it
 */
static void _add_irq_override(uint32_t gsi, uint16_t flags, uint8_t src, uint8_t bus)
{
    irq_override_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return;

    ret->gsi = gsi;
    ret->flags = flags;
    ret->src = src;
    ret->bus = bus;

    list_append(_irq_overrides, ret);
}

/*!
 * @brief: Remove an override entry from our list
 *
 * Releases the memory again
 */
static void _remove_irq_override(uint32_t gsi)
{
    irq_override_t* irq;

    FOREACH(i, _irq_overrides)
    {
        irq = i->data;

        if (irq->gsi == gsi)
            break;

        irq = nullptr;
    }

    if (!irq)
        return;

    list_remove_ex(_irq_overrides, irq);
    kfree(irq);
}

/*!
 * @brief: Try to get an IRQ override based on index
 */
int apic_get_irq_override(uint32_t idx, irq_override_t* boverride)
{
    irq_override_t* res;

    if (!boverride)
        return -KERR_INVAL;

    res = list_get(_irq_overrides, idx);

    if (!res)
        return -KERR_NOT_FOUND;

    /* Copy */
    memcpy(boverride, res, sizeof(*res));
    return 0;
}

int init_apic(apic_t** apic, size_t size, uint32_t base)
{
    int error;
    apic_t* ret = kmalloc(size);

    if (!ret)
        return -1;

    memset(ret, 0, size);

    ret->phys_register_base = base;
    ret->type = APIC_TYPE_LAPIC;
    ret->register_size = SMALL_PAGE_SIZE;
    ret->next = (apic_t*)(*apic);

    /* Map the local apic  */
    error = kmem_alloc((void**)&ret->register_base, NULL, NULL, base, SMALL_PAGE_SIZE, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE | KMEM_FLAG_NOCACHE);

    if (error) {
        kfree(ret);
        return error;
    }

    /* Set the thing */
    *apic = ret;
    return 0;
}

static void lapic_write(lapic_t* lapic, uint32_t reg, uint32_t val)
{
    *(volatile uint32_t*)(lapic->base.register_base + reg) = val;
}

static uint32_t lapic_read(lapic_t* lapic, uint32_t reg)
{
    return *(volatile uint32_t*)(lapic->base.register_base + reg);
}

static int _apic_spurious_handler(irq_t* irq)
{
    KLOG_DBG("APIC spurious called\n");
    return 0;
}

static int _apic_err_handler(irq_t* irq)
{
    KLOG_ERR("APIC error called\n");
    return 0;
}

static int _apic_ipi_handler(irq_t* irq)
{
    KLOG_DBG("APIC IPI called\n");
    return 0;
}

static inline void _apic_set_lvt(lapic_t* apic, uint32_t reg, uint32_t iv, uint32_t dm, uint32_t mask)
{
    lapic_write(apic, reg, (((iv) & 0xff) | (((dm) & 0x7) << 8)) | mask);
}

static void lapic_set_base(lapic_t* apic)
{
    uint64_t base;

    /* Grab the page-aligned base */
    base = apic->base.phys_register_base & 0xfffff000ULL;
    base |= (1 << 11);
    if (apic->is_x2)
        base |= (1 << 10);

    KLOG_DBG("LAPIC: Setting base: 0x%llx -> 0x%llx\n", (uint32_t)apic->base.phys_register_base, base);

    wrmsr(MSR_APIC_BASE, base);
}

static int enable_lapic(lapic_t* apic)
{
    uint32_t ld;

    KLOG_DBG("Enabling APIC on cpu: %d\n", apic->parent_cpu->m_cpu_num);

    /* TODO: Set X2 mode on this apic */
    if (apic->is_x2)
        apic->apic_id = lapic_read(apic, APIC_REG_ID);
    else {
        ld = lapic_read(apic, APIC_REG_LD) & 0x00ffffff;
        lapic_write(apic, APIC_REG_LD, ld | (apic->parent_cpu->m_cpu_num << 24));

        apic->apic_id = lapic_read(apic, APIC_REG_LD) >> 24;

        lapic_write(apic, APIC_REG_DF, 0xf0000000);
    }

    if (apic->parent_cpu->m_cpu_num == 0) {
        irq_allocate(IRQ_APIC_SPURIOUS, NULL, NULL, _apic_spurious_handler, NULL, "APIC spurious handler");
        irq_allocate(IRQ_APIC_ERR, NULL, NULL, _apic_err_handler, NULL, "APIC error handler");
        irq_allocate(IRQ_APIC_IPI, NULL, NULL, _apic_ipi_handler, NULL, "APIC IPI handler");
    }

    lapic_write(apic, APIC_REG_LVT_ERR, lapic_read(apic, APIC_REG_LVT_ERR) | IRQ_APIC_ERR);
    lapic_write(apic, APIC_REG_SIV, lapic_read(apic, APIC_REG_SIV) | IRQ_APIC_SPURIOUS | APIC_SOFTWARE_ENABLE);

    /* Set the LVT entries */
    _apic_set_lvt(apic, APIC_REG_LVT_TIMER, 0, 0, APIC_LVT_MASKED);
    _apic_set_lvt(apic, APIC_REG_LVT_THERMAL, 0, 0, APIC_LVT_MASKED);
    _apic_set_lvt(apic, APIC_REG_LVT_PERFORMANCE_COUNTER, 0, 0, APIC_LVT_MASKED);
    _apic_set_lvt(apic, APIC_REG_LVT_LINT0, 0, 7, APIC_LVT_MASKED);
    _apic_set_lvt(apic, APIC_REG_LVT_LINT1, 0, 0, APIC_LVT_TRIGGER_LEVEL);

    /* set the task prio register */
    lapic_write(apic, APIC_REG_TPR, 0);
    return 0;
}

static int init_lapic(lapic_t** apic, processor_t* cpu, uint32_t base)
{
    int error;
    lapic_t* ret;

    error = init_apic((apic_t**)apic, sizeof(lapic_t), base);

    if (error)
        return error;

    ret = *apic;

    ret->parent_cpu = cpu;
    ret->is_x2 = processor_has(&cpu->m_info, X86_FEATURE_X2APIC);
    ret->apic_id = lapic_read(ret, APIC_REG_ID) >> (ret->is_x2 ? 0 : 24);

    return 0;
}

int lapic_eoi(lapic_t* lapic)
{
    lapic_write(lapic, APIC_REG_EOI, 0);
    return 0;
}

static int _get_apics()
{
    int error;
    ssize_t len;
    acpi_parser_t* parser = NULL;
    acpi_tbl_madt_t* madt;
    ACPI_SUBTABLE_HEADER* subtable;

    get_root_acpi_parser(&parser);

    if (!parser)
        return -1;

    madt = acpi_parser_find_table_idx(parser, ACPI_SIG_MADT, 0, 0);

    if (!madt)
        return -1;

    error = init_lapic(&_bsp_lapic, &g_bsp, madt->Address);

    if (error)
        return error;

    len = madt->Header.Length - sizeof(*madt);
    subtable = (ACPI_SUBTABLE_HEADER*)((uint64_t)madt + sizeof(*madt));

    while (len > 0) {

        switch (subtable->Type) {
        case ACPI_MADT_TYPE_LOCAL_X2APIC: {
            ACPI_MADT_LOCAL_X2APIC* api = (ACPI_MADT_LOCAL_X2APIC*)subtable;

            KLOG_DBG("Found X2LAPIC on MADT. ID=%d\n", api->LocalApicId);
            _ap_count++;
            break;
        }
        case ACPI_MADT_TYPE_LOCAL_APIC: {
            ACPI_MADT_LOCAL_APIC* api = (ACPI_MADT_LOCAL_APIC*)subtable;

            KLOG_DBG("Found LAPIC on MADT. ID=%d\n", api->ProcessorId);
            _ap_count++;
            break;
        }
        case ACPI_MADT_TYPE_IO_APIC: {
            ACPI_MADT_IO_APIC* api = (ACPI_MADT_IO_APIC*)subtable;

            KLOG_DBG("Foudn IO APIC on MATD. IO=%d\n", api->Id);

            init_io_apic(&_io_apic, _bsp_lapic, api->Address, api->GlobalIrqBase);
            lapic_set_base(_bsp_lapic);
        }
        case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE: {
            ACPI_MADT_INTERRUPT_OVERRIDE* override = (ACPI_MADT_INTERRUPT_OVERRIDE*)subtable;

            KLOG_DBG("Interrupt override: GSI=%lld, FLAGS=%lld, SOURCEIRQ=%lld, BUS=%lld\n", override->GlobalIrq, override->IntiFlags, override->SourceIrq, override->Bus);

            /* Add the override */
            _add_irq_override(override->GlobalIrq, override->IntiFlags, override->SourceIrq, override->Bus);
        }
        }

        len -= subtable->Length;
        subtable = (ACPI_SUBTABLE_HEADER*)((vaddr_t)subtable + subtable->Length);
    }

    enable_lapic(_bsp_lapic);
    return 0;
}

/*!
 * @brief Initialize the BSP local apic
 *
 */
int init_apics()
{
    processor_t* cpu;

    cpu = get_current_processor();

    /* No apic supported */
    if (!processor_has(&cpu->m_info, X86_FEATURE_APIC))
        return -1;

    if (opt_parser_get_bool("force_pic"))
        return 1;

    /* Initialize the irq overrides list */
    _irq_overrides = init_list();

    /* Disable the fallback mofo */
    irq_disable_fallback_chip();

    /* Grab the fuckers */
    _get_apics();

    /* Update the system IRQ chip type */
    g_system_info.irq_chip_type = IRQ_CHIPTYPE_APIC;

    return 0;
}

irq_chip_t apic_controller = {};

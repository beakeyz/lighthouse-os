
#include "irq/ctl/ioapic.h"
#include "irq/ctl/apic.h"
#include "irq/ctl/irqchip.h"
#include "irq/interrupts.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include <stdint.h>

static void io_apic_write(io_apic_t* apic, uint32_t reg, uint32_t value)
{
    *(volatile uint32_t*)apic->base.register_base = reg;
    *(volatile uint32_t*)(apic->base.register_base + 0x10) = value;
}

static uint32_t io_apic_read(io_apic_t* apic, uint32_t reg)
{
    *(volatile uint32_t*)apic->base.register_base = reg;
    return *(volatile uint32_t*)(apic->base.register_base + 0x10);
}

static inline int __io_apic_mask_unmask_redirect(io_apic_t* apic, uint32_t idx, bool mask)
{
    uint32_t tmp;
    uint32_t reg;

    if (idx >= apic->redirect_count)
        return -KERR_NOT_FOUND;

    reg = IO_APIC_REDTBL_START_OFFSET + (1 << idx);
    tmp = io_apic_read(apic, reg);

    /*
     * Redirection entry already masked, no need to do
     * anything
     */
    if ((tmp & (1 << 16)) == mask)
        return 0;

    if (mask)
        tmp |= (1 << 16);
    else
        tmp &= ~(1 << 16);

    io_apic_write(apic, reg, tmp);
    return 0;
}

static int io_apic_mask_redirect(io_apic_t* apic, uint32_t idx)
{
    return __io_apic_mask_unmask_redirect(apic, idx, true);
}

static int io_apic_unmask_redirect(io_apic_t* apic, uint32_t idx)
{
    return __io_apic_mask_unmask_redirect(apic, idx, false);
}

static int io_apic_mask_all(irq_chip_t* chip)
{
    io_apic_t* apic = chip->private;

    /* Mask all redirection entries */
    for (uint32_t i = 0; i < apic->redirect_count; i++)
        io_apic_mask_redirect(apic, i);

    return 0;
}

static inline int __io_apic_conf_redirect(io_apic_t* apic, uint32_t idx, uint8_t vec, uint8_t mode, bool locical, bool act_lo, bool tlm, bool masked, uint8_t dest)
{
    uint32_t redirect_lo, redirect_hi;
    uint32_t reg;

    disable_interrupts();

    redirect_lo = vec | (mode & 0b111) << 8 | locical << 11 | act_lo << 13 | tlm << 15 | masked << 16;
    redirect_hi = dest << 24;
    reg = IO_APIC_REDTBL_START_OFFSET + (1 << idx);

    io_apic_write(apic, reg, redirect_lo);
    io_apic_write(apic, reg + 1, redirect_hi);

    enable_interrupts();

    return 0;
}

static int io_apic_map_redirect(io_apic_t* apic, uint32_t vec, uint32_t* idx)
{
    int error;
    uint32_t _idx;

    if (vec < IRQ_VEC_BASE)
        return -KERR_INVAL;

    _idx = (vec - IRQ_VEC_BASE);

    error = __io_apic_conf_redirect(apic, _idx, vec, 0, false, false, false, true, 0);

    if (error)
        return error;

    *idx = _idx;
    return 0;
}

static int io_apic_mask_vector(irq_chip_t* chip, uint32_t vec)
{
    return 0;
}

static int io_apic_unmask_vector(irq_chip_t* chip, uint32_t vec)
{
    uint32_t i;
    uint32_t c_vec;
    io_apic_t* apic = chip->private;

    for (i = 0; i < chip->irq_count; i++) {
        c_vec = (io_apic_read(apic, ((i) << 1) + IO_APIC_REDTBL_START_OFFSET) & 0xFF);

        KLOG_DBG("c_vec: %d\n", c_vec);

        if (c_vec == vec)
            break;
    }

    if (i == chip->irq_count)
        io_apic_map_redirect(apic, vec, &i);

    io_apic_mask_redirect(apic, i);
    return 0;
}

static void io_apic_eoi(irq_chip_t* chip, uint32_t vec)
{
    io_apic_t* apic = chip->private;
    /* Let the local APIC do the EOI */
    lapic_eoi(apic->lapic);
}

irq_chip_ops_t _io_apic_ops = {
    .f_mask_all = io_apic_mask_all,
    .f_mask_vec = io_apic_mask_vector,
    .f_unmask_vec = io_apic_unmask_vector,
    .ictl_eoi = io_apic_eoi,
};

int init_io_apic(io_apic_t** io_apic, lapic_t* lapic, uint32_t addr, uint32_t gsi_base)
{
    int error;
    io_apic_t* ret;

    error = init_apic((apic_t**)io_apic, addr);

    if (error)
        return error;

    ret = *io_apic;

    ret->lapic = lapic;
    ret->gsi_base = gsi_base;
    ret->id = (io_apic_read(ret, 0) >> 24) & 0xff;
    ret->version = (io_apic_read(ret, 1)) & 0xff;
    ret->redirect_count = (io_apic_read(ret, 1) >> 16) + 1;

    /* Initialize the chip fields */
    ret->chip.ops = &_io_apic_ops;
    ret->chip.private = ret;
    ret->chip.irq_base = gsi_base;
    ret->chip.irq_count = ret->redirect_count;

    KLOG_DBG("IO APIC ID=%d, VERSION=%d, REDIRECT_COUNT=%d GSI_BASE=%d\n", ret->id, ret->version, ret->redirect_count, gsi_base);

    /* Mask all fuckers */
    io_apic_mask_all(&ret->chip);

    /* Register the chip to the IRQ subsystem */
    irq_register_chip(&ret->chip);
    return error;
}

#include "pit.h"
#include "irq/interrupts.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "logging/log.h"
#include "time/core.h"
#include <system/asm_specifics.h>

#define PIT_BASE_FREQ 1193182

#define PIT_CTRL 0x43
#define T2_CTRL 0x42
#define T1_CTRL 0x41
#define T0_CTRL 0x40

// PIT modes
#define COUNTDOWN 0x00
#define OS 0x2
#define RATE 0x4
#define SQ_WAVE 0x6

// PIT cmds
#define T0_SEL 0x00
#define T1_SEL 0x40
#define T2_sel 0x80

#define PIT_WRITE 0x30

registers_t* pit_irq_handler(registers_t*);

static ANIVA_STATUS set_pit_interrupt_handler()
{
    int error;

    KLOG_DBG("Trying to allocate PIT irq\n");

    error = irq_allocate(PIT_TIMER_INT_NUM, NULL, NULL, pit_irq_handler, NULL, "Legacy PIT timer");

    ASSERT_MSG(error == 0, "Failed to allocate IRQ for the PIT timer =(");

    /* Ensure disabled */
    disable_interrupts();

    return ANIVA_SUCCESS;
}

static void set_pit_frequency(size_t freq, bool _enable_interrupts)
{
    disable_interrupts();
    const size_t new_val = PIT_BASE_FREQ / freq;
    out8(T0_CTRL, new_val & 0xff);
    out8(T0_CTRL, (new_val >> 8) & 0xff);

    if (_enable_interrupts)
        enable_interrupts();
}

static bool pit_frequency_capability(size_t freq)
{
    // FIXME
    return TARGET_TPS >= freq;
}

static void set_pit_periodic(bool value)
{
    // FIXME
}

// NOTE: we require the caller to enable interrupts again after this call
// I guarantee I will forget this some time so thats why this note is here
// for ez identification
static ALWAYS_INLINE void reset_pit(uint8_t mode)
{
    if (pit_frequency_capability(TARGET_TPS)) {
        out8(PIT_CTRL, T0_SEL | PIT_WRITE | mode);
        set_pit_frequency(TARGET_TPS, false);
    }
}

static int enable_pit(time_chip_t* chip)
{
    ANIVA_STATUS status;

    disable_interrupts();

    status = set_pit_interrupt_handler();

    if (status != ANIVA_SUCCESS)
        return -1;

    /* Reset this badboy */
    reset_pit(SQ_WAVE);

    return 0;
}

static int disable_pit(time_chip_t* chip)
{
    CHECK_AND_DO_DISABLE_INTERRUPTS();

    irq_deallocate(PIT_TIMER_INT_NUM, pit_irq_handler);

    CHECK_AND_TRY_ENABLE_INTERRUPTS();
    return 0;
}

time_chip_t pit_chip = {
    .type = PIT,
    .flags = NULL,
    .f_enable = enable_pit,
    .f_disable = disable_pit,
    .f_probe = NULL,
    .f_get_datetime = NULL,
};

registers_t* pit_irq_handler(registers_t* regs)
{
    (void)__do_tick(&pit_chip, regs, 1);
    return regs;
}

void init_pit_chip_driver()
{
    (void)time_register_chip(&pit_chip);
}

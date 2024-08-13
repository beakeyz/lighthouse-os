#include "pit.h"
#include "irq/interrupts.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
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

static u8 _pit_current_mode;
static u32 _pit_current_tps;

registers_t* pit_irq_handler(registers_t*);

static ANIVA_STATUS set_pit_interrupt_handler()
{
    int error;

    KLOG_DBG("Trying to allocate PIT irq\n");

    error = irq_allocate(PIT_TIMER_INT_NUM, NULL, NULL, pit_irq_handler, NULL, "Legacy PIT timer");

    ASSERT_MSG(error == 0, "Failed to allocate IRQ for the PIT timer =(");

    return ANIVA_SUCCESS;
}

static void set_pit_frequency(size_t tps, bool _enable_interrupts)
{
    disable_interrupts();

    const size_t new_val = PIT_BASE_FREQ / tps;

    /*
     * Fit the thing in the thing
     * This sets the counter base from which the PIT will
     * start decrementing every signal input.
     */
    out8(T0_CTRL, new_val & 0xff);
    out8(T0_CTRL, (new_val >> 8) & 0xff);

    if (_enable_interrupts)
        enable_interrupts();
}

static bool pit_frequency_capability(size_t freq)
{
    return TARGET_TPS >= freq;
}

// NOTE: we require the caller to enable interrupts again after this call
// I guarantee I will forget this some time so thats why this note is here
// for ez identification
static ALWAYS_INLINE void reset_pit(uint8_t mode, u32 tps)
{
    if (!pit_frequency_capability(TARGET_TPS))
        return;

    /* Enable the PIT */
    out8(PIT_CTRL, T0_SEL | PIT_WRITE | mode);

    _pit_current_mode = mode;

    /* Wait a lil bit */
    io_delay();

    _pit_current_tps = tps;

    /* Set the desired frequency */
    set_pit_frequency(tps, false);
}

static int enable_pit(time_chip_t* chip, u32 tps)
{
    ANIVA_STATUS status;

    /* Make sure interrupts are disabled */
    disable_interrupts();

    status = set_pit_interrupt_handler();

    if (status != ANIVA_SUCCESS)
        return -1;

    /* Reset this badboy */
    reset_pit(SQ_WAVE, tps);

    return 0;
}

static int disable_pit(time_chip_t* chip)
{
    CHECK_AND_DO_DISABLE_INTERRUPTS();

    irq_deallocate(PIT_TIMER_INT_NUM, pit_irq_handler);

    CHECK_AND_TRY_ENABLE_INTERRUPTS();
    return 0;
}

static int pit_get_systemtime(time_chip_t* chip, system_time_t* btime)
{
    u64 system_ticks;
    u64 tick_delta;

    if (!btime)
        return -1;

    /* Grab the system ticks */
    system_ticks = time_get_system_ticks();

    /* Calculate how many ticks have run past the second */
    tick_delta = system_ticks - ALIGN_DOWN(system_ticks, _pit_current_tps);

    /* Grab the amount of seconds since boot */
    btime->s_since_boot = ALIGN_DOWN(system_ticks, _pit_current_tps) / _pit_current_tps;
    /* Calculate the ms */
    btime->ms_since_last_s = (tick_delta * 1000) / _pit_current_tps;

    /* PIT does not know this xD */
    btime->boot_datetime = NULL;

    return 0;
}

time_chip_t pit_chip = {
    .type = PIT,
    .flags = NULL,
    .f_enable = enable_pit,
    .f_disable = disable_pit,
    .f_get_systemtime = pit_get_systemtime,
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
    _pit_current_tps = NULL;

    (void)time_register_chip(&pit_chip);
}

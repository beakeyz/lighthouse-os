#ifndef __APIC__
#define __APIC__
#include "irq/ctl/irqchip.h"
#include "system/processor/processor.h"
#include <libk/stddef.h>

#define APIC_BSP_BIT 8
#define APIC_GLOBAL_ENABLE_BIT 11
#define APIC_BASE_ADDRESS_MASK 0xFFFFF000

#define APIC_TIMER_LVT_OFFSET 0x00000320

#define APIC_LVT_TIMER_ONESHOT 0
#define APIC_LVT_TIMER_PERIODIC (1 << 17)
#define APIC_LVT_TIMER_TSCDEADLINE (1 << 18)
#define APIC_LVT_MASKED (1 << 16)
#define APIC_LVT_TRIGGER_LEVEL (1 << 14)

#define APIC_TABLE_LENGTH 0x6

#define APIC_TIMER_IDT_ENTRY 0x20

#define APIC_TIMER_DIVIDER_1 0xB
#define APIC_TIMER_DIVIDER_2 0x0
#define APIC_TIMER_DIVIDER_4 0x1
#define APIC_TIMER_DIVIDER_8 0x2
#define APIC_TIMER_DIVIDER_16 0x3
#define APIC_TIMER_DIVIDER_32 0x8
#define APIC_TIMER_DIVIDER_64 0x9
#define APIC_TIMER_DIVIDER_128 0xA

#define APIC_TIMER_MODE_ONE_SHOT 0x0
#define APIC_TIMER_MODE_PERIODIC 0x20000

#define APIC_SPURIOUS_INTERRUPT_IDT_ENTRY 0xFF
#define APIC_SOFTWARE_ENABLE (1 << 8)

#define APIC_REG_ID 0x20
#define APIC_REG_VERSION 0x30
#define APIC_REG_EOI 0xB0
#define APIC_REG_LD 0xd0
#define APIC_REG_DF 0xe0
#define APIC_REG_SIV 0xf0
#define APIC_REG_TPR 0x80
#define APIC_REG_ICR_LOW 0x300
#define APIC_REG_ICR_HIGH 0x310
#define APIC_REG_LVT_TIMER 0x320
#define APIC_REG_LVT_THERMAL 0x330
#define APIC_REG_LVT_PERFORMANCE_COUNTER 0x340
#define APIC_REG_LVT_LINT0 0x350
#define APIC_REG_LVT_LINT1 0x360
#define APIC_REG_LVT_ERR 0x370
#define APIC_REG_TIMER_INITIAL_COUNT 0x380
#define APIC_REG_TIMER_CURRENT_COUNT 0x390
#define APIC_REG_TIMER_CONFIGURATION 0x3e0

#define IRQ_APIC_TIMER (0xfc - 0x20)
#define IRQ_APIC_IPI (0xfd - 0x20)
#define IRQ_APIC_ERR (0xfe - 0x20)
#define IRQ_APIC_SPURIOUS (0xff - 0x20)

enum APIC_TYPE {
    APIC_TYPE_LAPIC,
    APIC_TYPE_IO_APIC,
};

struct apic;
struct lapic;
struct ioapic;

/*
 * Generic stuff
 * I love oop in C xD
 */
typedef struct apic {
    paddr_t phys_register_base;
    vaddr_t register_base;

    uint32_t register_size;
    enum APIC_TYPE type;

    struct apic* next;
} apic_t;

typedef struct io_apic {
    apic_t base;
    struct lapic* lapic;

    uint32_t gsi_base;
    uint32_t redirect_count;
    uint8_t id;
    uint8_t version;

    irq_chip_t chip;
} io_apic_t;

typedef struct lapic {
    apic_t base;
    processor_t* parent_cpu;
    bool is_x2;
    uint8_t apic_id;
} lapic_t;

int init_apics();
int init_apic(apic_t** apic, uint32_t base);
int lapic_eoi(lapic_t* lapic);

typedef struct {
    uint32_t gsi;
    uint16_t flags;
    uint8_t src;
    uint8_t bus;
} irq_override_t;

int apic_get_irq_override(uint32_t idx, irq_override_t* boverride);

#endif // !__APIC__

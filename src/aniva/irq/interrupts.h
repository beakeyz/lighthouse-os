#ifndef __ANIVA_INTERRUPTS__
#define __ANIVA_INTERRUPTS__

#include "irq/ctl/irqchip.h"
#include "system/asm_specifics.h"
#include "system/processor/registers.h"
#include <libk/stddef.h>

struct irq;
struct irq_handler;

// NOTE: this file CAN NOT include idt.h, because of an include loop =/

/* idt entry 0 -> 0x20 are reserved by exceptions */
#define IRQ_VEC_BASE 32
/* How many available IRQ vectors there are */
#define IRQ_COUNT (255 - IRQ_VEC_BASE)

#define IRQ_CHIPTYPE_PIT 0
#define IRQ_CHIPTYPE_APIC 1

/*
 * Initialize the interrupt subsystem
 */
void init_interrupts();

void disable_interrupts();
void enable_interrupts();

/*
 * NOTE: Can't be static
 */
inline bool interrupts_are_enabled()
{
  return ((get_eflags() & 0x200) != 0);
}

// hopefully this does not get used anywhere else ;-;
#define CHECK_AND_DO_DISABLE_INTERRUPTS()               \
  bool ___were_enabled_x = interrupts_are_enabled(); \
  disable_interrupts()                                 

#define CHECK_AND_TRY_ENABLE_INTERRUPTS()               \
  if (___were_enabled_x) {                              \
    enable_interrupts();                               \
  }

/*
 * Entrypoints for any interrupts
 */
registers_t* irq_handler (struct registers* regs);
registers_t* exception_handler (struct registers* regs);

/* The handler isn't passed any irq or register information, only the ctx */
#define IRQHANDLER_FLAG_DIRECT_CALL 0x00000001
/* offload the IRQ to the irq thread to be called asynchronously */
#define IRQHANDLER_FLAG_THREADED    0x00000002

typedef struct irq_handler {
  /* Handler to be called when the IRQ fires */
  union {
    int (*f_handle)(struct irq*, registers_t*);
    int (*f_handle_direct)(void* ctx);
  };

  /*
   * IRQ context. determined by the caller of irq_allocate and also owned by that caller 
   * This means that ->ctx should never be memory managed by IRQ code
   */
  void* ctx;
  const char* desc;

  uint32_t flags;
  struct irq_handler* next;
} irq_handler_t;

/*
 * Flags for irq entries
 */
#define IRQ_FLAG_ALLOCATED  0x00000001
/* Do we mean to use MSI with this vector? */
#define IRQ_FLAG_MSI        0x00000002
#define IRQ_FLAG_MASKED     0x00000004
#define IRQ_FLAG_SHOULD_DBG 0x00000008
/* 
 * The handler is currently masked, but we would like to unmask it =) 
 * There are certain times in the boot process that we scan the IRQs in order to find
 * irqs with this flag. This mostly happens when we're doing stuff with irq chips right
 * after they where disabled or something
 */
#define IRQ_FLAG_PENDING_UNMASK 0x00000010
#define IRQ_FLAG_SHARED         0x00000020

/*
 * IRQ template
 *
 * Contains information about what kind of system shit is assosiated with a given
 * IRQ vector. Contents of this struct are determined when the assosiated IRQ vector
 * is allocated
 */
typedef struct irq {
  uint32_t vec;
  uint32_t flags;

  /* The funky handlers */
  irq_handler_t* handlers;
} irq_t;

int irq_allocate(uint32_t vec, uint32_t irq_flags, uint32_t handler_flags, void* handler, void* ctx, const char* desc);
int irq_deallocate(uint32_t vec, void* handler);

int irq_mask(irq_t* vec);
int irq_unmask(irq_t* vec);

int irq_unmask_pending();

int irq_get(uint32_t vec, irq_t** irq);

int irq_lock();
int irq_unlock();

#endif // !__ANIVA_INTERRUPTS__


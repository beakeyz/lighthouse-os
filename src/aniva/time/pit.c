#include "pit.h"
#include "dev/debug/serial.h"
#include "interrupts/control/interrupt_control.h"
#include "interrupts/interrupts.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "time/core.h"
#include "sched/scheduler.h"
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

registers_t* pit_irq_handler (registers_t*);
ALWAYS_INLINE void reset_pit (uint8_t mode);
static size_t s_pit_ticks;

ANIVA_STATUS set_pit_interrupt_handler() {

  Must(install_quick_int_handler(PIT_TIMER_INT_NUM, QIH_FLAG_REGISTERED, I8259, pit_irq_handler));
  Must(quick_int_handler_enable_vector(PIT_TIMER_INT_NUM));

  /* Ensure disabled */
  disable_interrupts();

  return ANIVA_SUCCESS;
}

void set_pit_frequency(size_t freq, bool _enable_interrupts) {
  disable_interrupts();
  const size_t new_val = PIT_BASE_FREQ / freq;
  out8(T0_CTRL, new_val & 0xff);
  out8(T0_CTRL, (new_val >> 8) & 0xff);

  if (_enable_interrupts)
    enable_interrupts();
}

bool pit_frequency_capability(size_t freq) {
  // FIXME
  return TARGET_TPS >= freq;
}

void set_pit_periodic(bool value) {
  // FIXME
}

// NOTE: we require the caller to enable interrupts again after this call
// I guarantee I will forget this some time so thats why this note is here
// for ez identification
ALWAYS_INLINE void reset_pit (uint8_t mode) {
  if (pit_frequency_capability(TARGET_TPS)) {
    out8(PIT_CTRL, T0_SEL | PIT_WRITE | mode);
    set_pit_frequency(TARGET_TPS, false);
  }
}

void init_and_install_pit() {
  disable_interrupts();

  ANIVA_STATUS status = set_pit_interrupt_handler();
  if (status != ANIVA_SUCCESS) {
    return;
  }

  reset_pit(RATE);
}

void uninstall_pit() {
  CHECK_AND_DO_DISABLE_INTERRUPTS();

  uninstall_quick_int_handler(PIT_TIMER_INT_NUM);

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
}

size_t get_pit_ticks() {
  return s_pit_ticks;
}

registers_t* pit_irq_handler(registers_t* regs) {

  // FIXME: these calls do stuff to the heap. this means that when we get interrupted while
  // in some kind of heap call, a load of stuff can/will go wrong. we need to fix this in the future
  // by making sure there either is a lock on the heap (which might actually be a good idea anyways) or
  // by actually finishing the dynamic heap idea I have had for a while (being able to dynamically
  // create/remove different kinds of heaps (zone,blob,bump,ect.) which we can then call individually.
  // this would for this example mean that the kernel event system gets its own heap to allocate to.
  // we can give it a heaptype that is suitable to its kind of allocations and we also don't mess with the
  // main system heap. A big feature this heap should have is being able to be mapped to virtual ranges
  /*
  struct time_update_event_hook hook = create_time_update_event_hook(s_pit_ticks, regs);
  call_event(TIME_UPDATE_EVENT, &hook);
  */

  s_pit_ticks++;

  sched_tick(regs);

  return regs;
}

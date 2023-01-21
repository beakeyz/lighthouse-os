#include "pit.h"
#include "dev/debug/serial.h"
#include "interupts/control/interrupt_control.h"
#include "interupts/interupts.h"
#include "libk/error.h"
#include "libk/io.h"
#include "time/core.h"

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

ANIVA_STATUS set_pit_interrupt_handler() {

  InterruptHandler_t* handler = init_interrupt_handler(PIT_TIMER_INT_NUM, I8259, pit_irq_handler);
  bool success = add_handler(handler);

  if (success) {
    handler->m_controller->fControllerEnableVector(PIT_TIMER_INT_NUM);
    return ANIVA_SUCCESS;
  }

  // TODO
  return ANIVA_FAIL;
}

void set_pit_frequency(size_t freq, bool __enable_interupts) {
  disable_interrupts();
  const size_t new_val = PIT_BASE_FREQ / freq;
  out8(T0_CTRL, new_val & 0xff);
  out8(T0_CTRL, (new_val >> 8) & 0xff);

  if (__enable_interupts)
    enable_interrupts();
}

bool pit_frequency_capability(size_t freq) {
  // FIXME
  return true;
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
    set_pit_frequency(100, false);
  }
}

void init_and_install_pit() {
  disable_interrupts();

  ANIVA_STATUS status = set_pit_interrupt_handler();
  if (!status) {
    return;
  }

  reset_pit(RATE);

}

registers_t* pit_irq_handler(registers_t* regs) {

  print("hi");
  return regs;
}

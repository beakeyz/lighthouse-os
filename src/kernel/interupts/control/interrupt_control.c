#include "interrupt_control.h"
#include "interupts/control/pic.h"
#include "libk/linkedlist.h"
#include "mem/kmalloc.h"

InterruptControllerManager_t* g_interrupt_controller_manager;
static const int __pic_only = 1;

void init_int_control_management() {

  g_interrupt_controller_manager = kmalloc(sizeof(InterruptControllerManager_t));

  // TODO: check for available modes
  if (__pic_only) {
    interrupt_control_switch_to_mode(I8259);
  }
}

void interrupt_control_switch_to_mode(INTERRUPT_CONTROLLER_TYPE type) {
  g_interrupt_controller_manager->m_current_type = type;
  switch (type) {
    case I8259:
      // TODO: clear list and deactivate IOAPIC
      g_interrupt_controller_manager->m_controller_count = 1;
      PIC_t* pic = init_pic();
      list_append(&g_interrupt_controller_manager->m_controllers, &pic->m_controller);
      break;
    case I82093AA:
      // TODO: init IOAPIC
      break;
  }
}

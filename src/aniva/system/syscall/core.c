#include "core.h"
#include "system/processor/processor.h"

NAKED void sys_entry() {
  asm (
    "movq %%rsp, %%gs:%c[usr_stck_offset] \n"
    "movq %%gs:%c[krnl_stck_offset], %%rsp \n"
    // TODO:
    ""
    :
    :
    [krnl_stck_offset]"i"(get_kernel_stack_offset()),
    [usr_stck_offset]"i"(get_user_stack_offset())
    : "memory"
    );
}

void sys_handler(registers_t* regs) {

  Processor_t* current_processor = get_current_processor();
  thread_t *current_thread = get_current_scheduling_thread();

  kernel_panic("SUCCESS! recieved syscall!");

}
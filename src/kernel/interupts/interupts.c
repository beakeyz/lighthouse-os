#include "interupts.h"
#include "interupts/control/interrupt_control.h"
#include "kernel/interupts/idt.h"
#include "kernel/mem/kmem_manager.h"
#include "libk/error.h"
#include "libk/string.h"
#include <kernel/dev/debug/serial.h>
#include <kernel/interupts/control/pic.h>
#include <libk/stddef.h>
#include "mem/kmalloc.h"
#include "stubs.h"

#define INTERRUPT_HANDLER_COUNT (256 - 32)

static InterruptHandler_t* g_handlers[INTERRUPT_HANDLER_COUNT] = {NULL};

/* static defs */
static void insert_into_handlers(uint16_t index, InterruptHandler_t* handler);

// inspired by serenityOS
#define REGISTER_ERROR_HANDLER(code, title)                            \
extern void title##_asm_entry();                                       \
extern void title##_handler(registers_t*) __attribute__((used));       \
__attribute__((naked)) void title##_asm_entry() {                      \
  asm volatile (                                                       \
    "    pushq %rax\n"                                                 \
    "    pushq %rbx\n"                                                 \
    "    pushq %rcx\n"                                                 \
    "    pushq %rdx\n"                                                 \
    "    pushq %rsi\n"                                                 \
    "    pushq %rdi\n"                                                 \
    "    pushq %rbp\n"                                                 \
    "    pushq %r8\n"                                                  \
    "    pushq %r9\n"                                                  \
    "    pushq %r10\n"                                                 \
    "    pushq %r11\n"                                                 \
    "    pushq %r12\n"                                                 \
    "    pushq %r13\n"                                                 \
    "    pushq %r14\n"                                                 \
    "    pushq %r15\n"                                                 \
    "    cld\n"                                                        \
    "    movq %rsp, %rdi\n"                                            \
    "    call " #title "_handler\n"                                    \
    "    movq %rax, %rsp\n"                                            \
    "    popq %r15\n"                                                  \
    "    popq %r14\n"                                                  \
    "    popq %r13\n"                                                  \
    "    popq %r12\n"                                                  \
    "    popq %r11\n"                                                  \
    "    popq %r10\n"                                                  \
    "    popq %r9\n"                                                   \
    "    popq %r8\n"                                                   \
    "    popq %rbp\n"                                                  \
    "    popq %rdi\n"                                                  \
    "    popq %rsi\n"                                                  \
    "    popq %rdx\n"                                                  \
    "    popq %rcx\n"                                                  \
    "    popq %rbx\n"                                                  \
    "    popq %rax\n"                                                  \
    "    addq $16, %rsp\n" /* undo alignment */                        \
    "    iretq"                                                        \
  );                                                                   \
}

REGISTER_ERROR_HANDLER(14, pagefault);
void pagefault_handler(registers_t* regs) {
  print("error at ring: ");
  println(to_string(regs->cs & 3));
  kernel_panic("pagefault!");
}

InterruptHandler_t* init_interrupt_handler(uint16_t int_num, INTERRUPT_CONTROLLER_TYPE type, interrupt_callback_t callback) {

  InterruptHandler_t* ret = kmalloc(sizeof(InterruptHandler_t));

  InterruptController_t* controller_ptr = get_controller_for_int_number(int_num);

  if (controller_ptr->m_type != type) {
    return nullptr;
  }

  ret->m_int_num = int_num;
  ret->m_is_registerd = false;
  ret->m_controller = controller_ptr;
  ret->fHandler = callback;

  return ret;
}

// ???
InterruptHandler_t init_unhandled_interrupt_handler(uint16_t int_num) {
  InterruptHandler_t ret = {
    .fHandler = nullptr,
    .m_int_num = int_num,
    .m_controller = nullptr,
    .m_is_registerd = false
  };

  return ret;
}

static void insert_into_handlers(uint16_t index, InterruptHandler_t* handler) {
  handler->m_is_registerd = true;

  ASSERT(handler->m_int_num == index);

  g_handlers[index] = handler;
}

// heap should be up and running
void init_interupts() {
  // TODO: massive todo lol
  //        - find out what to do with exeptions
  //        - find out what to do with generic handlers
  //        - do we do function decleration in asm or c???
  //        - cry

  setup_idt(true);

  register_idt_interrupt_handler(32, (void(*)())interrupt_asm_entry_32);
  register_idt_interrupt_handler(33, (void(*)())interrupt_asm_entry_33);
  register_idt_interrupt_handler(34, (void(*)())interrupt_asm_entry_34);

  // prepare all waiting handlers
  for (uint8_t i = 0; i < INTERRUPT_HANDLER_COUNT; i++) {
    memset(g_handlers[i], 0x00, sizeof(InterruptHandler_t));
  }

  flush_idt();
}

// TODO: do stuff
void add_handler(InterruptHandler_t* handler_ptr) {
  disable_interupts();

  const uint16_t int_num = handler_ptr->m_int_num;

  InterruptHandler_t* handler = g_handlers[int_num];

  if (handler) {
    if (handler->m_is_registerd) {
      // TODO:
    }
  } else {
    insert_into_handlers(int_num, handler_ptr);
  }

  enable_interupts();
}

void remove_handler(const uint16_t int_num) {
  disable_interupts();

  InterruptHandler_t* handler = g_handlers[int_num];

  if (handler && handler->m_is_registerd) {
    if (handler->m_is_in_interrupt) {
      // hihi we tried to remove the handler while in an interrupt
      handler->m_controller->fInterruptEOI(int_num + 32); // we need to have the irq_num lmao
    }
    kfree(handler);
    g_handlers[int_num] = 0x00;
  }

  enable_interupts();
}

// main entrypoint for generinc interupts (from the asm)
registers_t *interrupt_handler(registers_t *regs) {
  // TODO: call _int_handler

  const uint8_t int_num = regs->isr_no - 32;
  InterruptHandler_t* handler = g_handlers[int_num];

  // TODO: surpious n crap
  if (handler && handler->m_is_registerd) {

    handler->m_is_in_interrupt = true;
    handler->fHandler(regs);

    // you never know lmao
    if (handler->m_controller)
      handler->m_controller->fInterruptEOI(regs->isr_no);
  }

  return regs;
}

void disable_interupts() { asm volatile("cli"); }

void enable_interupts() { asm volatile("sti"); }

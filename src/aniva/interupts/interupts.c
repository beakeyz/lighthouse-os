#include "interupts.h"
#include "interupts/control/interrupt_control.h"
#include "interupts/idt.h"
#include "mem/kmem_manager.h"
#include "libk/error.h"
#include "libk/string.h"
#include <dev/debug/serial.h>
#include <interupts/control/pic.h>
#include <libk/stddef.h>
#include "mem/kmalloc.h"
#include "stubs.h"
#include "system/asm_specifics.h"
#include "sched/scheduler.h"

// TODO: linked list for dynamic handler loading?
static InterruptHandler_t *g_handlers[INTERRUPT_HANDLER_COUNT] = {NULL};

/* static defs */
static void insert_into_handlers(uint16_t index, InterruptHandler_t *handler);

// inspired by serenityOS
#define REGISTER_ERROR_HANDLER(code, title)                            \
extern void title##_asm_entry();                                       \
extern void title##_handler(registers_t*) __attribute__((used));       \
__attribute__((naked)) void title##_asm_entry() {                      \
  asm volatile (                                                       \
    "    pushq $0x00\n"                                                \
    "    pushq %r15\n"                                                 \
    "    pushq %r14\n"                                                 \
    "    pushq %r13\n"                                                 \
    "    pushq %r12\n"                                                 \
    "    pushq %r11\n"                                                 \
    "    pushq %r10\n"                                                 \
    "    pushq %r9\n"                                                  \
    "    pushq %r8\n"                                                  \
    "    pushq %rax\n"                                                 \
    "    pushq %rbx\n"                                                 \
    "    pushq %rcx\n"                                                 \
    "    pushq %rdx\n"                                                 \
    "    pushq %rbp\n"                                                 \
    "    pushq %rsi\n"                                                 \
    "    pushq %rdi\n"                                                 \
    "    cld\n"                                                        \
    "    movq %rsp, %rdi\n"                                            \
    "    call " #title "_handler\n"                                    \
    "    movq %rax, %rsp\n"                                            \
    "    popq %rdi\n"                                                  \
    "    popq %rsi\n"                                                  \
    "    popq %rbp\n"                                                  \
    "    popq %rdx\n"                                                  \
    "    popq %rcx\n"                                                  \
    "    popq %rbx\n"                                                  \
    "    popq %rax\n"                                                  \
    "    popq %r8\n"                                                   \
    "    popq %r9\n"                                                   \
    "    popq %r10\n"                                                  \
    "    popq %r11\n"                                                  \
    "    popq %r12\n"                                                  \
    "    popq %r13\n"                                                  \
    "    popq %r14\n"                                                  \
    "    popq %r15\n"                                                  \
    "    addq $16, %rsp\n" /* undo alignment */                        \
    "    iretq"                                                        \
  );                                                                   \
}

// extra stackpush in order to fix up the registers_t structure
#define REGISTER_ERROR_HANDLER_NO_CODE(code, title)                    \
extern void title##_asm_entry();                                       \
extern void title##_handler(registers_t*) __attribute__((used));       \
__attribute__((naked)) void title##_asm_entry() {                      \
  asm volatile (                                                       \
    "    pushq $0x00\n"                                                \
    "    pushq $0x00\n"                                                \
    "    pushq %r15\n"                                                 \
    "    pushq %r14\n"                                                 \
    "    pushq %r13\n"                                                 \
    "    pushq %r12\n"                                                 \
    "    pushq %r11\n"                                                 \
    "    pushq %r10\n"                                                 \
    "    pushq %r9\n"                                                  \
    "    pushq %r8\n"                                                  \
    "    pushq %rax\n"                                                 \
    "    pushq %rbx\n"                                                 \
    "    pushq %rcx\n"                                                 \
    "    pushq %rdx\n"                                                 \
    "    pushq %rbp\n"                                                 \
    "    pushq %rsi\n"                                                 \
    "    pushq %rdi\n"                                                 \
    "    cld\n"                                                        \
    "    movq %rsp, %rdi\n"                                            \
    "    call " #title "_handler\n"                                    \
    "    movq %rax, %rsp\n"                                            \
    "    popq %rdi\n"                                                  \
    "    popq %rsi\n"                                                  \
    "    popq %rbp\n"                                                  \
    "    popq %rdx\n"                                                  \
    "    popq %rcx\n"                                                  \
    "    popq %rbx\n"                                                  \
    "    popq %rax\n"                                                  \
    "    popq %r8\n"                                                   \
    "    popq %r9\n"                                                   \
    "    popq %r10\n"                                                  \
    "    popq %r11\n"                                                  \
    "    popq %r12\n"                                                  \
    "    popq %r13\n"                                                  \
    "    popq %r14\n"                                                  \
    "    popq %r15\n"                                                  \
    "    addq $16, %rsp\n" /* undo alignment */                        \
    "    iretq"                                                        \
  );                                                                   \
}

// TODO: more info on the processor state
#define EXCEPTION(number, error)           \
static void __exception_##number() {       \
  kernel_panic(error);                    \
}                                         \

/*
    EXCEPTION stubs
    TODO: move to seperate file?
*/

REGISTER_ERROR_HANDLER_NO_CODE(0, devision_by_zero);

void devision_by_zero_handler(registers_t *regs) {
  kernel_panic("devision by zero!");
}

// TODO: debug user-entry
REGISTER_ERROR_HANDLER_NO_CODE(1, debug);
void debug_handler(registers_t* regs) {
  println("debug trap triggered");
  print("cs: ");
  println(to_string(regs->cs));
  print("eflags: ");
  println(to_string(regs->rflags));
  if ((regs->cs & 3) == 0) {
    kernel_panic("yeet");
  }
}

EXCEPTION(2, "Unknown error (non-maskable interrupt)");

// TODO: breakpoint user-entry

EXCEPTION(4, "Overflow");

EXCEPTION(5, "Bounds range exceeded");

REGISTER_ERROR_HANDLER_NO_CODE(6, illegal_instruction);

void illegal_instruction_handler(registers_t *regs) {

}

REGISTER_ERROR_HANDLER_NO_CODE(7, fpu);

void fpu_handler(registers_t *regs) {

}

EXCEPTION(8, "Double fault");

EXCEPTION(9, "Segment overrun");

EXCEPTION(10, "Invalid TSS");

EXCEPTION(11, "Segment not present");

EXCEPTION(12, "Stack segment fault");

REGISTER_ERROR_HANDLER(13, general_protection);

void general_protection_handler(registers_t *regs) {
  kernel_panic("General protection fault (TODO: more info)");
}

REGISTER_ERROR_HANDLER(14, pagefault);

void pagefault_handler(registers_t *regs) {

  uintptr_t err_addr = read_cr2();
  uintptr_t cs = regs->cs;

  println(" --- PAGEFAULT --- ");
  print("error at ring: ");
  println(to_string(cs & 3));
  print("error at addr: ");
  println(to_string(err_addr));
  if (regs->err_code & 8) {
    println("Reserved!");
  }
  if (regs->err_code & 2) {
    println("write");
  } else {
    println("read");
  }
  println((regs->err_code & 1) ? "PV" : "NP");

  kernel_panic("pagefault! (TODO: more info)");
}

EXCEPTION(15, "Unknown error (Reserved)");
EXCEPTION(16, "X87 fp exception");
EXCEPTION(17, "Alignment check failed");

EXCEPTION(18, "Machine check exception");

EXCEPTION(19, "SIMD floating point exception");

EXCEPTION(20, "Virtualization exception");

EXCEPTION(21, "Control Protection exception");

EXCEPTION(28, "Hypervisor injection exception");

EXCEPTION(29, "VMM Communication exception");

EXCEPTION(30, "Security exception");

static void __unimplemented_EXCEPTION() {
  kernel_panic("Unimplemented EXCEPTION");
}

static void __unimplemented_EXCEPTION_a() {
  kernel_panic("alt");
}

/*
   INTERRUPT HANDLER
*/

InterruptHandler_t *
init_interrupt_handler(uint16_t int_num, INTERRUPT_CONTROLLER_TYPE type, interrupt_callback_t callback) {

  InterruptHandler_t *ret = kmalloc(sizeof(InterruptHandler_t));

  InterruptController_t *controller_ptr = get_controller_for_int_number(int_num);

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

static void insert_into_handlers(uint16_t index, InterruptHandler_t *handler) {
  ASSERT(handler);
  ASSERT(handler->m_int_num == index);

  handler->m_is_registerd = true;
  g_handlers[index] = handler;
}

/*
   INTERRUPTS
*/

// heap should be up and running
void init_interupts() {
  // TODO: massive todo lol
  //        - find out what to do with EXCEPTIONs
  //        - find out what to do with generic handlers
  //        - do we do function decleration in asm or c???
  //        - cry

  setup_idt(true);

  register_idt_interrupt_handler(0x00, devision_by_zero_asm_entry);
  register_idt_interrupt_handler(0x01, debug_asm_entry);
  register_idt_interrupt_handler(0x02, __exception_2);
  register_idt_interrupt_handler(0x03, __unimplemented_EXCEPTION);
  register_idt_interrupt_handler(0x04, __exception_4);
  register_idt_interrupt_handler(0x05, __exception_5);
  register_idt_interrupt_handler(0x06, illegal_instruction_asm_entry);
  register_idt_interrupt_handler(0x07, fpu_asm_entry);
  register_idt_interrupt_handler(0x08, __exception_8);
  register_idt_interrupt_handler(0x09, __exception_9);
  register_idt_interrupt_handler(0x0a, __exception_10);
  register_idt_interrupt_handler(0x0b, __exception_11);
  register_idt_interrupt_handler(0x0c, __exception_12);
  register_idt_interrupt_handler(0x0d, general_protection_asm_entry);
  register_idt_interrupt_handler(0x0e, pagefault_asm_entry);
  register_idt_interrupt_handler(0x0f, __exception_15);
  register_idt_interrupt_handler(0x10, __exception_16);
  register_idt_interrupt_handler(0x11, __exception_17);
  register_idt_interrupt_handler(0x12, __exception_18);
  register_idt_interrupt_handler(0x13, __exception_19);
  register_idt_interrupt_handler(0x14, __exception_20);
  register_idt_interrupt_handler(0x15, __exception_21);

  for (uint8_t i = 0x16; i <= 0x1B; i++) {
    register_idt_interrupt_handler(i, __unimplemented_EXCEPTION);
  }

  register_idt_interrupt_handler(0x1C, __exception_28);
  register_idt_interrupt_handler(0x1D, __exception_29);
  register_idt_interrupt_handler(0x1E, __exception_30);

  register_idt_interrupt_handler(0x1F, __unimplemented_EXCEPTION);

  register_idt_interrupt_handler(0x20, (FuncPtr) interrupt_asm_entry_32);
  register_idt_interrupt_handler(0x21, (FuncPtr) interrupt_asm_entry_33);
  register_idt_interrupt_handler(0x22, (FuncPtr) interrupt_asm_entry_34);
  register_idt_interrupt_handler(0x23, (FuncPtr) interrupt_asm_entry_35);
  register_idt_interrupt_handler(0x24, (FuncPtr) interrupt_asm_entry_36);
  register_idt_interrupt_handler(0x25, (FuncPtr) interrupt_asm_entry_37);
  register_idt_interrupt_handler(0x26, (FuncPtr) interrupt_asm_entry_38);
  register_idt_interrupt_handler(0x27, (FuncPtr) interrupt_asm_entry_39);
  register_idt_interrupt_handler(0x28, (FuncPtr) interrupt_asm_entry_40);
  register_idt_interrupt_handler(0x29, (FuncPtr) interrupt_asm_entry_41);
  register_idt_interrupt_handler(0x2a, (FuncPtr) interrupt_asm_entry_42);
  register_idt_interrupt_handler(0x2b, (FuncPtr) interrupt_asm_entry_43);
  register_idt_interrupt_handler(0x2c, (FuncPtr) interrupt_asm_entry_44);
  register_idt_interrupt_handler(0x2d, (FuncPtr) interrupt_asm_entry_45);
  register_idt_interrupt_handler(0x2e, (FuncPtr) interrupt_asm_entry_46);
  register_idt_interrupt_handler(0x2f, (FuncPtr) interrupt_asm_entry_47);
  register_idt_interrupt_handler(0x30, (FuncPtr) interrupt_asm_entry_48);
  register_idt_interrupt_handler(0x31, (FuncPtr) interrupt_asm_entry_49);
  register_idt_interrupt_handler(0x32, (FuncPtr) interrupt_asm_entry_50);
  register_idt_interrupt_handler(0x33, (FuncPtr) interrupt_asm_entry_51);
  register_idt_interrupt_handler(0x34, (FuncPtr) interrupt_asm_entry_52);
  register_idt_interrupt_handler(0x35, (FuncPtr) interrupt_asm_entry_53);
  register_idt_interrupt_handler(0x36, (FuncPtr) interrupt_asm_entry_54);
  register_idt_interrupt_handler(0x37, (FuncPtr) interrupt_asm_entry_55);
  register_idt_interrupt_handler(0x38, (FuncPtr) interrupt_asm_entry_56);
  register_idt_interrupt_handler(0x39, (FuncPtr) interrupt_asm_entry_57);
  register_idt_interrupt_handler(0x3a, (FuncPtr) interrupt_asm_entry_58);
  register_idt_interrupt_handler(0x3b, (FuncPtr) interrupt_asm_entry_59);
  register_idt_interrupt_handler(0x3c, (FuncPtr) interrupt_asm_entry_60);
  register_idt_interrupt_handler(0x3d, (FuncPtr) interrupt_asm_entry_61);
  register_idt_interrupt_handler(0x3e, (FuncPtr) interrupt_asm_entry_62);
  register_idt_interrupt_handler(0x3f, (FuncPtr) interrupt_asm_entry_63);
  register_idt_interrupt_handler(0x40, (FuncPtr) interrupt_asm_entry_64);
  register_idt_interrupt_handler(0x41, (FuncPtr) interrupt_asm_entry_65);
  register_idt_interrupt_handler(0x42, (FuncPtr) interrupt_asm_entry_66);
  register_idt_interrupt_handler(0x43, (FuncPtr) interrupt_asm_entry_67);
  register_idt_interrupt_handler(0x44, (FuncPtr) interrupt_asm_entry_68);
  register_idt_interrupt_handler(0x45, (FuncPtr) interrupt_asm_entry_69);
  register_idt_interrupt_handler(0x46, (FuncPtr) interrupt_asm_entry_70);
  register_idt_interrupt_handler(0x47, (FuncPtr) interrupt_asm_entry_71);
  register_idt_interrupt_handler(0x48, (FuncPtr) interrupt_asm_entry_72);
  register_idt_interrupt_handler(0x49, (FuncPtr) interrupt_asm_entry_73);
  register_idt_interrupt_handler(0x4a, (FuncPtr) interrupt_asm_entry_74);
  register_idt_interrupt_handler(0x4b, (FuncPtr) interrupt_asm_entry_75);
  register_idt_interrupt_handler(0x4c, (FuncPtr) interrupt_asm_entry_76);
  register_idt_interrupt_handler(0x4d, (FuncPtr) interrupt_asm_entry_77);
  register_idt_interrupt_handler(0x4e, (FuncPtr) interrupt_asm_entry_78);
  register_idt_interrupt_handler(0x4f, (FuncPtr) interrupt_asm_entry_79);
  register_idt_interrupt_handler(0x50, (FuncPtr) interrupt_asm_entry_80);
  register_idt_interrupt_handler(0x51, (FuncPtr) interrupt_asm_entry_81);
  register_idt_interrupt_handler(0x52, (FuncPtr) interrupt_asm_entry_82);
  register_idt_interrupt_handler(0x53, (FuncPtr) interrupt_asm_entry_83);
  register_idt_interrupt_handler(0x54, (FuncPtr) interrupt_asm_entry_84);
  register_idt_interrupt_handler(0x55, (FuncPtr) interrupt_asm_entry_85);
  register_idt_interrupt_handler(0x56, (FuncPtr) interrupt_asm_entry_86);
  register_idt_interrupt_handler(0x57, (FuncPtr) interrupt_asm_entry_87);
  register_idt_interrupt_handler(0x58, (FuncPtr) interrupt_asm_entry_88);
  register_idt_interrupt_handler(0x59, (FuncPtr) interrupt_asm_entry_89);
  register_idt_interrupt_handler(0x5a, (FuncPtr) interrupt_asm_entry_90);
  register_idt_interrupt_handler(0x5b, (FuncPtr) interrupt_asm_entry_91);
  register_idt_interrupt_handler(0x5c, (FuncPtr) interrupt_asm_entry_92);
  register_idt_interrupt_handler(0x5d, (FuncPtr) interrupt_asm_entry_93);
  register_idt_interrupt_handler(0x5e, (FuncPtr) interrupt_asm_entry_94);
  register_idt_interrupt_handler(0x5f, (FuncPtr) interrupt_asm_entry_95);
  register_idt_interrupt_handler(0x60, (FuncPtr) interrupt_asm_entry_96);
  register_idt_interrupt_handler(0x61, (FuncPtr) interrupt_asm_entry_97);
  register_idt_interrupt_handler(0x62, (FuncPtr) interrupt_asm_entry_98);
  register_idt_interrupt_handler(0x63, (FuncPtr) interrupt_asm_entry_99);
  register_idt_interrupt_handler(0x64, (FuncPtr) interrupt_asm_entry_100);
  register_idt_interrupt_handler(0x65, (FuncPtr) interrupt_asm_entry_101);
  register_idt_interrupt_handler(0x66, (FuncPtr) interrupt_asm_entry_102);
  register_idt_interrupt_handler(0x67, (FuncPtr) interrupt_asm_entry_103);
  register_idt_interrupt_handler(0x68, (FuncPtr) interrupt_asm_entry_104);
  register_idt_interrupt_handler(0x69, (FuncPtr) interrupt_asm_entry_105);
  register_idt_interrupt_handler(0x6a, (FuncPtr) interrupt_asm_entry_106);
  register_idt_interrupt_handler(0x6b, (FuncPtr) interrupt_asm_entry_107);
  register_idt_interrupt_handler(0x6c, (FuncPtr) interrupt_asm_entry_108);
  register_idt_interrupt_handler(0x6d, (FuncPtr) interrupt_asm_entry_109);
  register_idt_interrupt_handler(0x6e, (FuncPtr) interrupt_asm_entry_110);
  register_idt_interrupt_handler(0x6f, (FuncPtr) interrupt_asm_entry_111);
  register_idt_interrupt_handler(0x70, (FuncPtr) interrupt_asm_entry_112);
  register_idt_interrupt_handler(0x71, (FuncPtr) interrupt_asm_entry_113);
  register_idt_interrupt_handler(0x72, (FuncPtr) interrupt_asm_entry_114);
  register_idt_interrupt_handler(0x73, (FuncPtr) interrupt_asm_entry_115);
  register_idt_interrupt_handler(0x74, (FuncPtr) interrupt_asm_entry_116);
  register_idt_interrupt_handler(0x75, (FuncPtr) interrupt_asm_entry_117);
  register_idt_interrupt_handler(0x76, (FuncPtr) interrupt_asm_entry_118);
  register_idt_interrupt_handler(0x77, (FuncPtr) interrupt_asm_entry_119);
  register_idt_interrupt_handler(0x78, (FuncPtr) interrupt_asm_entry_120);
  register_idt_interrupt_handler(0x79, (FuncPtr) interrupt_asm_entry_121);
  register_idt_interrupt_handler(0x7a, (FuncPtr) interrupt_asm_entry_122);
  register_idt_interrupt_handler(0x7b, (FuncPtr) interrupt_asm_entry_123);
  register_idt_interrupt_handler(0x7c, (FuncPtr) interrupt_asm_entry_124);
  register_idt_interrupt_handler(0x7d, (FuncPtr) interrupt_asm_entry_125);
  register_idt_interrupt_handler(0x7e, (FuncPtr) interrupt_asm_entry_126);
  register_idt_interrupt_handler(0x7f, (FuncPtr) interrupt_asm_entry_127);
  register_idt_interrupt_handler(0x80, (FuncPtr) interrupt_asm_entry_128);
  register_idt_interrupt_handler(0x81, (FuncPtr) interrupt_asm_entry_129);
  register_idt_interrupt_handler(0x82, (FuncPtr) interrupt_asm_entry_130);
  register_idt_interrupt_handler(0x83, (FuncPtr) interrupt_asm_entry_131);
  register_idt_interrupt_handler(0x84, (FuncPtr) interrupt_asm_entry_132);
  register_idt_interrupt_handler(0x85, (FuncPtr) interrupt_asm_entry_133);
  register_idt_interrupt_handler(0x86, (FuncPtr) interrupt_asm_entry_134);
  register_idt_interrupt_handler(0x87, (FuncPtr) interrupt_asm_entry_135);
  register_idt_interrupt_handler(0x88, (FuncPtr) interrupt_asm_entry_136);
  register_idt_interrupt_handler(0x89, (FuncPtr) interrupt_asm_entry_137);
  register_idt_interrupt_handler(0x8a, (FuncPtr) interrupt_asm_entry_138);
  register_idt_interrupt_handler(0x8b, (FuncPtr) interrupt_asm_entry_139);
  register_idt_interrupt_handler(0x8c, (FuncPtr) interrupt_asm_entry_140);
  register_idt_interrupt_handler(0x8d, (FuncPtr) interrupt_asm_entry_141);
  register_idt_interrupt_handler(0x8e, (FuncPtr) interrupt_asm_entry_142);
  register_idt_interrupt_handler(0x8f, (FuncPtr) interrupt_asm_entry_143);
  register_idt_interrupt_handler(0x90, (FuncPtr) interrupt_asm_entry_144);
  register_idt_interrupt_handler(0x91, (FuncPtr) interrupt_asm_entry_145);
  register_idt_interrupt_handler(0x92, (FuncPtr) interrupt_asm_entry_146);
  register_idt_interrupt_handler(0x93, (FuncPtr) interrupt_asm_entry_147);
  register_idt_interrupt_handler(0x94, (FuncPtr) interrupt_asm_entry_148);
  register_idt_interrupt_handler(0x95, (FuncPtr) interrupt_asm_entry_149);
  register_idt_interrupt_handler(0x96, (FuncPtr) interrupt_asm_entry_150);
  register_idt_interrupt_handler(0x97, (FuncPtr) interrupt_asm_entry_151);
  register_idt_interrupt_handler(0x98, (FuncPtr) interrupt_asm_entry_152);
  register_idt_interrupt_handler(0x99, (FuncPtr) interrupt_asm_entry_153);
  register_idt_interrupt_handler(0x9a, (FuncPtr) interrupt_asm_entry_154);
  register_idt_interrupt_handler(0x9b, (FuncPtr) interrupt_asm_entry_155);
  register_idt_interrupt_handler(0x9c, (FuncPtr) interrupt_asm_entry_156);
  register_idt_interrupt_handler(0x9d, (FuncPtr) interrupt_asm_entry_157);
  register_idt_interrupt_handler(0x9e, (FuncPtr) interrupt_asm_entry_158);
  register_idt_interrupt_handler(0x9f, (FuncPtr) interrupt_asm_entry_159);
  register_idt_interrupt_handler(0xa0, (FuncPtr) interrupt_asm_entry_160);
  register_idt_interrupt_handler(0xa1, (FuncPtr) interrupt_asm_entry_161);
  register_idt_interrupt_handler(0xa2, (FuncPtr) interrupt_asm_entry_162);
  register_idt_interrupt_handler(0xa3, (FuncPtr) interrupt_asm_entry_163);
  register_idt_interrupt_handler(0xa4, (FuncPtr) interrupt_asm_entry_164);
  register_idt_interrupt_handler(0xa5, (FuncPtr) interrupt_asm_entry_165);
  register_idt_interrupt_handler(0xa6, (FuncPtr) interrupt_asm_entry_166);
  register_idt_interrupt_handler(0xa7, (FuncPtr) interrupt_asm_entry_167);
  register_idt_interrupt_handler(0xa8, (FuncPtr) interrupt_asm_entry_168);
  register_idt_interrupt_handler(0xa9, (FuncPtr) interrupt_asm_entry_169);
  register_idt_interrupt_handler(0xaa, (FuncPtr) interrupt_asm_entry_170);
  register_idt_interrupt_handler(0xab, (FuncPtr) interrupt_asm_entry_171);
  register_idt_interrupt_handler(0xac, (FuncPtr) interrupt_asm_entry_172);
  register_idt_interrupt_handler(0xad, (FuncPtr) interrupt_asm_entry_173);
  register_idt_interrupt_handler(0xae, (FuncPtr) interrupt_asm_entry_174);
  register_idt_interrupt_handler(0xaf, (FuncPtr) interrupt_asm_entry_175);
  register_idt_interrupt_handler(0xb0, (FuncPtr) interrupt_asm_entry_176);
  register_idt_interrupt_handler(0xb1, (FuncPtr) interrupt_asm_entry_177);
  register_idt_interrupt_handler(0xb2, (FuncPtr) interrupt_asm_entry_178);
  register_idt_interrupt_handler(0xb3, (FuncPtr) interrupt_asm_entry_179);
  register_idt_interrupt_handler(0xb4, (FuncPtr) interrupt_asm_entry_180);
  register_idt_interrupt_handler(0xb5, (FuncPtr) interrupt_asm_entry_181);
  register_idt_interrupt_handler(0xb6, (FuncPtr) interrupt_asm_entry_182);
  register_idt_interrupt_handler(0xb7, (FuncPtr) interrupt_asm_entry_183);
  register_idt_interrupt_handler(0xb8, (FuncPtr) interrupt_asm_entry_184);
  register_idt_interrupt_handler(0xb9, (FuncPtr) interrupt_asm_entry_185);
  register_idt_interrupt_handler(0xba, (FuncPtr) interrupt_asm_entry_186);
  register_idt_interrupt_handler(0xbb, (FuncPtr) interrupt_asm_entry_187);
  register_idt_interrupt_handler(0xbc, (FuncPtr) interrupt_asm_entry_188);
  register_idt_interrupt_handler(0xbd, (FuncPtr) interrupt_asm_entry_189);
  register_idt_interrupt_handler(0xbe, (FuncPtr) interrupt_asm_entry_190);
  register_idt_interrupt_handler(0xbf, (FuncPtr) interrupt_asm_entry_191);
  register_idt_interrupt_handler(0xc0, (FuncPtr) interrupt_asm_entry_192);
  register_idt_interrupt_handler(0xc1, (FuncPtr) interrupt_asm_entry_193);
  register_idt_interrupt_handler(0xc2, (FuncPtr) interrupt_asm_entry_194);
  register_idt_interrupt_handler(0xc3, (FuncPtr) interrupt_asm_entry_195);
  register_idt_interrupt_handler(0xc4, (FuncPtr) interrupt_asm_entry_196);
  register_idt_interrupt_handler(0xc5, (FuncPtr) interrupt_asm_entry_197);
  register_idt_interrupt_handler(0xc6, (FuncPtr) interrupt_asm_entry_198);
  register_idt_interrupt_handler(0xc7, (FuncPtr) interrupt_asm_entry_199);
  register_idt_interrupt_handler(0xc8, (FuncPtr) interrupt_asm_entry_200);
  register_idt_interrupt_handler(0xc9, (FuncPtr) interrupt_asm_entry_201);
  register_idt_interrupt_handler(0xca, (FuncPtr) interrupt_asm_entry_202);
  register_idt_interrupt_handler(0xcb, (FuncPtr) interrupt_asm_entry_203);
  register_idt_interrupt_handler(0xcc, (FuncPtr) interrupt_asm_entry_204);
  register_idt_interrupt_handler(0xcd, (FuncPtr) interrupt_asm_entry_205);
  register_idt_interrupt_handler(0xce, (FuncPtr) interrupt_asm_entry_206);
  register_idt_interrupt_handler(0xcf, (FuncPtr) interrupt_asm_entry_207);
  register_idt_interrupt_handler(0xd0, (FuncPtr) interrupt_asm_entry_208);
  register_idt_interrupt_handler(0xd1, (FuncPtr) interrupt_asm_entry_209);
  register_idt_interrupt_handler(0xd2, (FuncPtr) interrupt_asm_entry_210);
  register_idt_interrupt_handler(0xd3, (FuncPtr) interrupt_asm_entry_211);
  register_idt_interrupt_handler(0xd4, (FuncPtr) interrupt_asm_entry_212);
  register_idt_interrupt_handler(0xd5, (FuncPtr) interrupt_asm_entry_213);
  register_idt_interrupt_handler(0xd6, (FuncPtr) interrupt_asm_entry_214);
  register_idt_interrupt_handler(0xd7, (FuncPtr) interrupt_asm_entry_215);
  register_idt_interrupt_handler(0xd8, (FuncPtr) interrupt_asm_entry_216);
  register_idt_interrupt_handler(0xd9, (FuncPtr) interrupt_asm_entry_217);
  register_idt_interrupt_handler(0xda, (FuncPtr) interrupt_asm_entry_218);
  register_idt_interrupt_handler(0xdb, (FuncPtr) interrupt_asm_entry_219);
  register_idt_interrupt_handler(0xdc, (FuncPtr) interrupt_asm_entry_220);
  register_idt_interrupt_handler(0xdd, (FuncPtr) interrupt_asm_entry_221);
  register_idt_interrupt_handler(0xde, (FuncPtr) interrupt_asm_entry_222);
  register_idt_interrupt_handler(0xdf, (FuncPtr) interrupt_asm_entry_223);
  register_idt_interrupt_handler(0xe0, (FuncPtr) interrupt_asm_entry_224);
  register_idt_interrupt_handler(0xe1, (FuncPtr) interrupt_asm_entry_225);
  register_idt_interrupt_handler(0xe2, (FuncPtr) interrupt_asm_entry_226);
  register_idt_interrupt_handler(0xe3, (FuncPtr) interrupt_asm_entry_227);
  register_idt_interrupt_handler(0xe4, (FuncPtr) interrupt_asm_entry_228);
  register_idt_interrupt_handler(0xe5, (FuncPtr) interrupt_asm_entry_229);
  register_idt_interrupt_handler(0xe6, (FuncPtr) interrupt_asm_entry_230);
  register_idt_interrupt_handler(0xe7, (FuncPtr) interrupt_asm_entry_231);
  register_idt_interrupt_handler(0xe8, (FuncPtr) interrupt_asm_entry_232);
  register_idt_interrupt_handler(0xe9, (FuncPtr) interrupt_asm_entry_233);
  register_idt_interrupt_handler(0xea, (FuncPtr) interrupt_asm_entry_234);
  register_idt_interrupt_handler(0xeb, (FuncPtr) interrupt_asm_entry_235);
  register_idt_interrupt_handler(0xec, (FuncPtr) interrupt_asm_entry_236);
  register_idt_interrupt_handler(0xed, (FuncPtr) interrupt_asm_entry_237);
  register_idt_interrupt_handler(0xee, (FuncPtr) interrupt_asm_entry_238);
  register_idt_interrupt_handler(0xef, (FuncPtr) interrupt_asm_entry_239);
  register_idt_interrupt_handler(0xf0, (FuncPtr) interrupt_asm_entry_240);
  register_idt_interrupt_handler(0xf1, (FuncPtr) interrupt_asm_entry_241);
  register_idt_interrupt_handler(0xf2, (FuncPtr) interrupt_asm_entry_242);
  register_idt_interrupt_handler(0xf3, (FuncPtr) interrupt_asm_entry_243);
  register_idt_interrupt_handler(0xf4, (FuncPtr) interrupt_asm_entry_244);
  register_idt_interrupt_handler(0xf5, (FuncPtr) interrupt_asm_entry_245);
  register_idt_interrupt_handler(0xf6, (FuncPtr) interrupt_asm_entry_246);
  register_idt_interrupt_handler(0xf7, (FuncPtr) interrupt_asm_entry_247);
  register_idt_interrupt_handler(0xf8, (FuncPtr) interrupt_asm_entry_248);
  register_idt_interrupt_handler(0xf9, (FuncPtr) interrupt_asm_entry_249);
  register_idt_interrupt_handler(0xfa, (FuncPtr) interrupt_asm_entry_250);
  register_idt_interrupt_handler(0xfb, (FuncPtr) interrupt_asm_entry_251);
  register_idt_interrupt_handler(0xfc, (FuncPtr) interrupt_asm_entry_252);
  register_idt_interrupt_handler(0xfd, (FuncPtr) interrupt_asm_entry_253);
  register_idt_interrupt_handler(0xfe, (FuncPtr) interrupt_asm_entry_254);
  register_idt_interrupt_handler(0xff, (FuncPtr) interrupt_asm_entry_255);

  // prepare all waiting handlers
  for (uint8_t i = 0; i < INTERRUPT_HANDLER_COUNT; i++) {
    memset(g_handlers[i], 0x00, sizeof(InterruptHandler_t));
  }

  flush_idt();
}

/*
   MORE HANDLER SHIT
*/

// TODO: do stuff
bool add_handler(InterruptHandler_t *handler_ptr) {

  CHECK_AND_DO_DISABLE_INTERRUPTS();

  const uint16_t int_num = handler_ptr->m_int_num;
  const InterruptHandler_t *handler = g_handlers[int_num];

  bool success = true;

  // this flow is kinda weird
  // if we have a registered handler, we assume the caller knows this
  // and thus we also assume the caller just want to replace the current
  // handler. if there somehow is a unregistered handler, we just skip the
  // opperation alltogether and let the caller know it failed...
  if (handler) {
    if (handler->m_is_registerd) {
      remove_handler(int_num);
    } else {
      success = false;
      goto skip_insert;
    }
  }

  insert_into_handlers(int_num, handler_ptr);

  // lil exit label
  skip_insert:
  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return success;
}

void remove_handler(const uint16_t int_num) {

  CHECK_AND_DO_DISABLE_INTERRUPTS();

  InterruptHandler_t *handler = g_handlers[int_num];

  if (handler && handler->m_is_registerd) {
    if (handler->m_is_in_interrupt) {
      // hihi we tried to remove the handler while in an interrupt
      handler->m_controller->fInterruptEOI(int_num + 32); // we need to have the irq_num lmao
    }
    kfree(handler);
    g_handlers[int_num] = 0x00;
  }

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
}

// main entrypoint for generinc interupts (from the asm)
registers_t *interrupt_handler(registers_t *regs) {

  const uint8_t int_num = regs->isr_no - 32;

  InterruptHandler_t *handler = g_handlers[int_num];

  // TODO: surpious n crap
  if (handler && handler->m_is_registerd) {

    // you never know lmao
    if (handler->m_controller)
      handler->m_controller->fInterruptEOI(regs->isr_no);

    handler->m_is_in_interrupt = true;
    regs = handler->fHandler(regs);
  }

  return regs;
}

void disable_interrupts() {
  asm volatile(
    "cli"::
    : "memory"
    );
}

void enable_interrupts() {
  asm volatile(
    "sti"::
    : "memory"
    );
}

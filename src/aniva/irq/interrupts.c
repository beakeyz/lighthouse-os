#include "interrupts.h"
#include "irq/ctl/irqchip.h"
#include "irq/faults/faults.h"
#include "irq/idt.h"
#include "logging/log.h"
#include "libk/flow/error.h"
#include <dev/debug/serial.h>
#include <libk/stddef.h>
#include <mem/heap.h>
#include "proc/proc.h"
#include "stubs.h"
#include "sync/mutex.h"
#include "sched/scheduler.h"

static mutex_t* _irq_lock;
static irq_t _irq_list[IRQ_COUNT] = { 0 };

int irq_get(uint32_t vec, irq_t** irq)
{
  if (!irq)
    return -1;

  if (vec >= IRQ_COUNT)
    return -2;

  *irq = &_irq_list[vec];
  return 0;
}

/*!
 * @brief: Take the _irq_lock
 * 
 * Used if externals want to interact with interrupts (Like PCI)
 */
int irq_lock()
{
  mutex_lock(_irq_lock);
  return 0;
}

/*!
 * @brief: Release the _irq_lock
 * Used if externals want to interact with interrupts (Like PCI)
 */
int irq_unlock()
{
  mutex_unlock(_irq_lock);
  return 0;
}

static inline bool irq_is_allocated(irq_t* irq)
{
  return ((irq->flags & IRQ_FLAG_ALLOCATED) == IRQ_FLAG_ALLOCATED);
}

static inline bool irq_should_debug(irq_t* irq)
{
  return ((irq->flags & IRQ_FLAG_SHOULD_DBG) == IRQ_FLAG_SHOULD_DBG);
}

static int _irq_allocate(irq_t* irq, uint32_t flags, void* handler, void* ctx, const char* desc)
{
  irq_handler_t* n_handler;

  n_handler = kmalloc(sizeof(*n_handler));

  if (!n_handler)
    return -1;

  memset(n_handler, 0, sizeof(*n_handler));

  n_handler->flags = flags;
  n_handler->f_handle = handler;
  n_handler->ctx = ctx;
  n_handler->desc = desc;

  /* Link */
  n_handler->next = irq->handlers;

  /* Put ourselves in the front */
  irq->handlers = n_handler;

  return 0;
}

/*!
 * @brief: Allocate an IRQ on the vector @vec
 *
 * NOTE: Also unmasks the vector in the current active interrupt chip if the IRQ_FLAG_MASKED flag
 * isn't given through @flags
 *
 * 
 */
int irq_allocate(uint32_t vec, uint32_t irq_flags, uint32_t handler_flags, void* handler, void* ctx, const char* desc)
{
  int error;
  irq_t* i;

  mutex_lock(_irq_lock);

  error = irq_get(vec, &i);

  if (error)
    goto alloc_exit;

  error = -1;

  /* If not shared and allocated, we can't allocate, yikes */
  if ((i->flags & IRQ_FLAG_SHARED) != IRQ_FLAG_SHARED && irq_is_allocated(i))
    goto alloc_exit;

  /* Actually allocate */
  error = _irq_allocate(i, handler_flags, handler, ctx, desc);

  if (error)
    goto alloc_exit;

  i->vec = vec;

  /* Only update the flags if this IRQ is not yet allocated */
  if (!irq_is_allocated(i))
    i->flags = IRQ_FLAG_ALLOCATED | irq_flags;

  if ((irq_flags & IRQ_FLAG_MASKED) == IRQ_FLAG_MASKED)
    goto alloc_exit;

  error = irq_unmask(i);

alloc_exit:
  mutex_unlock(_irq_lock);
  return error;
}

/*!
 * @brief: Little routine for if irt_t ever owns its own memory at some point
 */
static inline void destroy_irq(irq_t* irq)
{
  irq_handler_t* c_handler;

  c_handler = irq->handlers;

  while (c_handler) {
    void* handler = c_handler;
    c_handler = c_handler->next;

    kfree(handler);
  }

  /* TODO: If there is any memory that @irq gets ownership of, destroy it here */
  memset(irq, 0, sizeof(*irq));
}

int irq_deallocate(uint32_t vec, void* handler)
{
  int error;
  irq_t* irq;
  irq_handler_t* target_handler;
  irq_handler_t** c_handler;

  error = irq_get(vec, &irq);

  if (error)
    return -1;

  /* Make sure no one fucks us */
  mutex_lock(_irq_lock);

  /* Mask the irq so we don't catch any accidental interrupts */
  irq_mask(irq);

  /* Clear out the entire irq */
  if (!irq->handlers || !irq->handlers->next)
    destroy_irq(irq);
  else {
    c_handler = &irq->handlers;
    target_handler = nullptr;

    do {
      /* We looking for this handler? */
      if ((*c_handler)->f_handle != handler)
        goto cycle;
      
      target_handler = *c_handler;
      *c_handler = (*c_handler)->next;

      kfree(target_handler);
      break;
cycle:
      c_handler = &(*c_handler)->next;
    } while(*c_handler);

    if (!target_handler)
      error = -KERR_INVAL;
  }

  /* We're done =) */
  mutex_unlock(_irq_lock);
  return error;
}

int irq_unmask_pending()
{
  irq_t* c_irq;

  for (uint32_t i = 0; i < IRQ_COUNT; i++) {
    irq_get(i, &c_irq);

    if ((c_irq->flags & IRQ_FLAG_PENDING_UNMASK) != IRQ_FLAG_PENDING_UNMASK)
      continue;

    irq_unmask(c_irq);
  }

  return 0;
}

/*! 
 * @brief: Mask a singulare IRQ vector
 *
 * Contacts the current active irq management chip to ask it to mask the vector
 * this makes the system ignore any IRQ fires of this vector after the mask
 *
 * _irq_lock should be taken when this is called
 */
int irq_mask(irq_t* irq)
{
  irq_chip_t* c;

  if (get_active_irq_chip(&c))
    return -1;

  /*
   * Clear this just in case. When an IRQ is masked, it shouldn't 
   * randomly get unmasked again lmao
   */
  irq->flags &= ~IRQ_FLAG_PENDING_UNMASK;
  /* Make sure we know this bitch is masked */
  irq->flags |= IRQ_FLAG_MASKED;

  return irq_chip_mask(c, irq->vec);
}

/*! 
 * @brief: Unmask a singulare IRQ vector
 *
 * Contacts the current active irq management chip to ask it to unmask the vector
 * this makes the system accept any IRQ fires of this vector after the unmask
 *
 * _irq_lock should be taken when this is called
 */
int irq_unmask(irq_t* irq)
{
  irq_chip_t* c;

  if (get_active_irq_chip(&c))
    return -1;

  /* Clear this just in case */
  irq->flags &= ~IRQ_FLAG_PENDING_UNMASK;
  /* Make sure we know this bitch is masked */
  irq->flags &= ~IRQ_FLAG_MASKED;

  return irq_chip_unmask(c, irq->vec);
}

#define REGISTER_IDT_EXCEPTION_ENTRY(num) \
  register_idt_interrupt_handler((num), (FuncPtr)(interrupt_excp_asm_entry_##num))

static inline void install_exception_stubs()
{
  REGISTER_IDT_EXCEPTION_ENTRY(0);
  REGISTER_IDT_EXCEPTION_ENTRY(1);
  REGISTER_IDT_EXCEPTION_ENTRY(2);
  REGISTER_IDT_EXCEPTION_ENTRY(3);
  REGISTER_IDT_EXCEPTION_ENTRY(4);
  REGISTER_IDT_EXCEPTION_ENTRY(5);
  REGISTER_IDT_EXCEPTION_ENTRY(6);
  REGISTER_IDT_EXCEPTION_ENTRY(7);
  REGISTER_IDT_EXCEPTION_ENTRY(8);
  REGISTER_IDT_EXCEPTION_ENTRY(9);
  REGISTER_IDT_EXCEPTION_ENTRY(10);
  REGISTER_IDT_EXCEPTION_ENTRY(11);
  REGISTER_IDT_EXCEPTION_ENTRY(12);
  REGISTER_IDT_EXCEPTION_ENTRY(13);
  REGISTER_IDT_EXCEPTION_ENTRY(14);
  REGISTER_IDT_EXCEPTION_ENTRY(15);
  REGISTER_IDT_EXCEPTION_ENTRY(16);
  REGISTER_IDT_EXCEPTION_ENTRY(17);
  REGISTER_IDT_EXCEPTION_ENTRY(18);
  REGISTER_IDT_EXCEPTION_ENTRY(19);
  REGISTER_IDT_EXCEPTION_ENTRY(20);
  REGISTER_IDT_EXCEPTION_ENTRY(21);
  REGISTER_IDT_EXCEPTION_ENTRY(22);
  REGISTER_IDT_EXCEPTION_ENTRY(23);
  REGISTER_IDT_EXCEPTION_ENTRY(24);
  REGISTER_IDT_EXCEPTION_ENTRY(25);
  REGISTER_IDT_EXCEPTION_ENTRY(26);
  REGISTER_IDT_EXCEPTION_ENTRY(27);
  REGISTER_IDT_EXCEPTION_ENTRY(28);
  REGISTER_IDT_EXCEPTION_ENTRY(29);
  REGISTER_IDT_EXCEPTION_ENTRY(30);
  REGISTER_IDT_EXCEPTION_ENTRY(31);
}

static inline void install_regular_stubs()
{
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
}

/*!
 * @brief: Initialize the interrupt subsystem + x86 arch specifics
 *
 * We don't enable interrupts here. The IRQ chip(s) get initialized during late processor
 * initialization
 */
void init_interrupts() 
{
  /* Setup the IDT structure */
  setup_idt(true);

  _irq_lock = create_mutex(NULL);

  /* Install stubs for exceptions */
  install_exception_stubs();

  /* Install the regular IRQ stubs */
  install_regular_stubs();

  /* Load the complete IDT */
  flush_idt();
}

static inline uint8_t _get_irq_vector(uintptr_t isr_num)
{
  if (isr_num > 0xffULL)
    return 0xff;

  if (isr_num < 32)
    return 0xff;

  return isr_num - IRQ_VEC_BASE;
}

static inline void irq_debug(irq_t* irq, int handler_error)
{
  (void)irq;
  (void)handler_error;
  kernel_panic("TODO: debug irqs");
}

/*!
 * @brief: Tell the active irq management chip we are done with this interrupt
 *
 * Figures out the current IRQ chip and orders it to ACK
 */
static inline void irq_ack(irq_t* irq)
{
  irq_chip_t* chip;

  if (get_active_irq_chip(&chip))
    return;

  irq_chip_ack(chip, irq);
}

static inline int _irq_handle(irq_t* irq, irq_handler_t* handler, registers_t* regs)
{
  if ((handler->flags & IRQHANDLER_FLAG_THREADED) == IRQHANDLER_FLAG_THREADED)
    kernel_panic("TODO: Threaded IRQs");

  if ((handler->flags & IRQHANDLER_FLAG_DIRECT_CALL) == IRQHANDLER_FLAG_DIRECT_CALL)
    return handler->f_handle_direct(handler->ctx);

  return handler->f_handle(irq, regs);
}

static inline int irq_handle(irq_t* irq, registers_t* regs)
{
  int error;
  irq_handler_t* c_handler;

  c_handler = irq->handlers;
  
  do {
    /* Do the actual handling */
    error = _irq_handle(irq, c_handler, regs);

    if (error)
      break;

    c_handler = c_handler->next;
  } while (c_handler);

  return error;
}

/*!
 * @brief: main entrypoint for generinc interrupts (from the asm)
 *
 * FIXME: don't return registers, its kinda unsafe lmao
 */
registers_t *irq_handler(registers_t *regs) 
{
  int error;
  irq_t* irq;
  uint32_t irq_vec;

  irq_vec = _get_irq_vector(regs->isr_no);

  /* We can assert here yay */
  ASSERT_MSG(irq_get(irq_vec, &irq) == 0, "Recieved an interrupt with an invalid IRQ vector!");

  /* 
   * Unused irq should not be called (But how is it unmasked?) 
   * FIXME: report loose irqs
   */
  if (!irq_is_allocated(irq) || !irq->handlers)
    goto acknowlege;

  /* Try to handle the IRQ in the requested way */
  error = irq_handle(irq, regs);

  /* Only debug if that's asked of us */
  if (irq_should_debug(irq))
    irq_debug(irq, error);

acknowlege:
  /* Acknowlege the IRQ to our management chip so we can recieve another IRQ */
  irq_ack(irq);
  return regs;
}

/*!
 * @brief: Main exception handler
 *
 * Looks at the isr_no and determines the appropriate handler to run
 * If it's (For some reason) a fault it doesn't know, it throws a fatal
 * kernel error
 *
 * NOTE: I want to handle interrupt exceptions and kernel errors differently. When something
 * like an essential memory allocation fails, it needs to eventually land on the same result as
 * a fatal exception: our version of the 'bluescreen' which gives the user some info on what went
 * wrong. The question is just how do we mix those different scenarios together.
 */
registers_t* exception_handler (struct registers* regs)
{
  enum FAULT_RESULT result;
  aniva_fault_t fault;
  uint32_t exc_num;
  proc_t* c_proc;

  exc_num = regs->isr_no;

  KLOG_ERR("Got isr no (%lld) and err no (%lld)\n", regs->isr_no, regs->err_code);

  if (get_fault(exc_num, &fault))
    kernel_panic("Got an exception but failed to get the fault handler!");

  c_proc = get_current_proc();

  result = try_handle_fault(&fault, regs);

  switch (result) {
    case FR_KILL_PROC:
      Must(try_terminate_process(c_proc));
      break;
    case FR_FATAL:
      kernel_panic("Failed to handle fatal fault!");
    default:
      break;
  }

  /* Fault should have been handled! */
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

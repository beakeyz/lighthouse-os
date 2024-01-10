#ifndef __ANIVA_IRQ_FAULTS__
#define __ANIVA_IRQ_FAULTS__

#include "system/processor/registers.h"

/*
 * A few possible results from a fault
 *
 * For example, a pagefault in a userprocess shouldn't panic the kernel, but simply close
 * the faulty process
 * 
 * NOTE: faults from drivers are always fatal
 */
enum FAULT_RESULT {
  FR_FATAL = 0,
  /* Fault completely resolved */
  FR_RESOLVED,
  /* The current process fucked up. Kill it */
  FR_KILL_PROC,
  /* Silent fault, skip any debugging */
  FR_SKIP_DEBUG,
  /* Semi-silent fault, skip some debugging */
  FR_DO_PARTIAL_DEBUG,
};

/*
 * Generic fault types
 *
 * Not every exception IRQ has it's own fault type, but they are
 * all brought under their own categories
 */
enum FAULT_TYPE {
  FT_PAGEFAULT = 0,
  FT_GPF,
  FT_DEVIDE_BY_ZERO,
  FT_FLOATING_POINT,
  FT_MEMORY,
  FT_SECURITY,
  FT_FATAL,
  FT_DEBUG,
  FT_GENERIC,
};

/* Should this fault fire an error kevent */
#define FAULT_FLAG_FIRE_EVENT 0x00000001
/* Should we do a complete system dump when this fault happens */
#define FAULT_FLAG_DEBUG_DUMP 0x00000002
/* Should we print a traceback when this fault happens */
#define FAULT_FLAG_TRACEBACK  0x00000004
/* Should we hang or reboot when we fail to resolve this fault */
#define FAULT_FLAG_REBOOT_ON_FAIL 0x00000008
/* Should we print extra debug info? */
#define FAULT_FLAG_PRINT_DEBUG 0x00000010

/*
 * Generic system fault
 *
 */
typedef struct aniva_fault {
  uint32_t irq_num;
  uint32_t flags;
  enum FAULT_TYPE type;

  const char* name;

  /* Assess the damage */
  enum FAULT_RESULT (*f_assess)(const struct aniva_fault*, registers_t* ctx);
  /* Handle the fault */
  enum FAULT_RESULT (*f_handle)(const struct aniva_fault*, registers_t* ctx);
} aniva_fault_t;

extern int generate_traceback(registers_t* registers);

extern int get_fault(uint32_t irq_num, aniva_fault_t* out);
extern enum FAULT_RESULT try_handle_fault(aniva_fault_t* fault, registers_t* ctx);

#endif // !__ANIVA_IRQ_FAULTS__

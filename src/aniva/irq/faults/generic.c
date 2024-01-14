#include "faults.h"
#include "libk/bin/ksyms.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include <logging/log.h>

/*
 * These generic fault should be handler right here and most of the
 * time they should just continue execution
 */

/* Div by zero, no assess */
static enum FAULT_RESULT div_by_zero_handler(const aniva_fault_t* fault, registers_t* regs);
/* Debug trap, no assess */
static enum FAULT_RESULT debug_trap_handler(const aniva_fault_t* fault, registers_t* regs);
/* Breakpoint, no assess */
static enum FAULT_RESULT breakpoint_handler(const aniva_fault_t* fault, registers_t* regs);
/* Illegal instruction, no assess */
static enum FAULT_RESULT illegal_instr_handler(const aniva_fault_t* fault, registers_t* regs);
/* Invalid TSS */
static enum FAULT_RESULT tss_err_handler(const aniva_fault_t* fault, registers_t* regs);
/* Segment not present */
static enum FAULT_RESULT seg_not_present_handler(const aniva_fault_t* fault, registers_t* regs);
/* Stack seg fault */
static enum FAULT_RESULT stack_seg_handler(const aniva_fault_t* fault, registers_t* regs);
/* Reserved */
static enum FAULT_RESULT reserved_irq_handler(const aniva_fault_t* fault, registers_t* regs);

/*
 * Security faults mostly just kill the current process
 */

/* Machine check */
extern enum FAULT_RESULT machine_chk_handler(const aniva_fault_t* fault, registers_t* regs);
/* Virtualization */
extern enum FAULT_RESULT virt_handler(const aniva_fault_t* fault, registers_t* regs);
/* Control protection */
extern enum FAULT_RESULT control_prot_handler(const aniva_fault_t* fault, registers_t* regs);
/* Hypervisor injection */
extern enum FAULT_RESULT hyprv_inj_handler(const aniva_fault_t* fault, registers_t* regs);
/* VMM communication */
extern enum FAULT_RESULT vmm_com_handler(const aniva_fault_t* fault, registers_t* regs);
/* Security */
extern enum FAULT_RESULT security_fault_handler(const aniva_fault_t* fault, registers_t* regs);

/* FPU error, no assess */
extern enum FAULT_RESULT fpu_err_handler(const aniva_fault_t* fault, registers_t* regs);
/* x87 FPU */
extern enum FAULT_RESULT x87_fpu_err_handler(const aniva_fault_t* fault, registers_t* regs);
/* SIMD FPU */
extern enum FAULT_RESULT simd_fpu_err_handler(const aniva_fault_t* fault, registers_t* regs);

/*
 * Memory faults
 */

/* Alignment failure */
extern enum FAULT_RESULT alignment_handler(const aniva_fault_t* fault, registers_t* regs);

/* Segment overrun */
extern enum FAULT_RESULT seg_overrun_handler(const aniva_fault_t* fault, registers_t* regs);

/* Bounds range */
extern enum FAULT_RESULT boundrange_handler(const aniva_fault_t* fault, registers_t* regs);

/* Overflow */
extern enum FAULT_RESULT overflow_handler(const aniva_fault_t* fault, registers_t* regs);

/*
 * (Probably) fatal faults
 */

/* Double fault */
extern enum FAULT_RESULT doublefault_handler(const aniva_fault_t* fault, registers_t* regs);

/* NMI, no assess */
extern enum FAULT_RESULT unknown_err_handler(const aniva_fault_t* fault, registers_t* regs);

/* PF */
extern enum FAULT_RESULT pagefault_handler(const aniva_fault_t* fault, registers_t* regs);

/* GPF */
extern enum FAULT_RESULT gpf_handler(const aniva_fault_t* fault, registers_t* regs);

/*
static enum FAULT_RESULT generic_skip_debug_asses(const aniva_fault_t* fault, registers_t* regs)
{
  return FR_SKIP_DEBUG;
}
*/

static const aniva_fault_t _static_faults[] = {
  {
    0,
    0,
    FT_DEVIDE_BY_ZERO,
    "Devide by zero",
    div_by_zero_handler,
  },
  {
    1,
    0,
    FT_DEBUG,
    "Debug trap",
    debug_trap_handler,
  },
  {
    2,
    0,
    FT_FATAL,
    "Unknown error (NMI)",
    unknown_err_handler,
  },
  {
    3,
    0,
    FT_DEBUG,
    "Breakpoint",
    breakpoint_handler,
  },
  {
    4,
    0,
    FT_SECURITY,
    "Overflow",
    overflow_handler
  },
  {
    5,
    0,
    FT_MEMORY,
    "Bounds range exceeded",
    boundrange_handler,
  },
  {
    6,
    0,
    FT_SECURITY,
    "Illegal instruction",
    illegal_instr_handler,
  },
  {
    7,
    0,
    FT_FPU,
    "Generic FPU error",
    fpu_err_handler,
  },
  {
    8,
    0,
    FT_FATAL,
    "Double fault",
    doublefault_handler,
  },
  {
    9,
    0,
    FT_MEMORY,
    "Segment overrun",
    seg_overrun_handler,
  },
  {
    10,
    0,
    FT_MEMORY,
    "Invalid TSS",
    tss_err_handler,
  },
  {
    11,
    0,
    FT_MEMORY,
    "Segment not present",
    seg_not_present_handler,
  },
  {
    12,
    0,
    FT_MEMORY,
    "Stack segment fault",
    stack_seg_handler,
  },
  {
    13,
    0,
    FT_GPF,
    "General protection fault",
    gpf_handler,
  },
  {
    14,
    FAULT_FLAG_PRINT_DEBUG | FAULT_FLAG_TRACEBACK,
    FT_PAGEFAULT,
    "Pagefault",
    pagefault_handler,
  },
  {
    15,
    0,
    FT_RESERVED,
    "Reserved 0",
    reserved_irq_handler,
  },
  {
    16,
    0,
    FT_FPU,
    "x87 FPU fault",
    x87_fpu_err_handler,
  },
  {
    17,
    0,
    FT_MEMORY,
    "Alignment fault",
    alignment_handler,
  },
  {
    18,
    0,
    FT_SECURITY,
    "Machine check",
    machine_chk_handler,
  },
  {
    19,
    0,
    FT_FPU,
    "SIMD FPU fault",
    simd_fpu_err_handler,
  },
  {
    20,
    0,
    FT_SECURITY,
    "Virtualization",
    virt_handler,
  },
  {
    21,
    0,
    FT_SECURITY,
    "Control protection",
    control_prot_handler,
  },
  {
    22,
    0,
    FT_RESERVED,
    "Reserved 1",
    reserved_irq_handler,
  },
  {
    23,
    0,
    FT_RESERVED,
    "Reserved 2",
    reserved_irq_handler,
  },
  {
    24,
    0,
    FT_RESERVED,
    "Reserved 3",
    reserved_irq_handler,
  },
  {
    25,
    0,
    FT_RESERVED,
    "Reserved 4",
    reserved_irq_handler,
  },
  {
    26,
    0,
    FT_RESERVED,
    "Reserved 5",
    reserved_irq_handler,
  },
  {
    27,
    0,
    FT_RESERVED,
    "Reserved 6",
    reserved_irq_handler,
  },
  {
    28,
    0,
    FT_SECURITY,
    "Hypervisor injection",
    hyprv_inj_handler,
  },
  {
    29,
    FAULT_FLAG_PRINT_DEBUG | FAULT_FLAG_TRACEBACK,
    FT_SECURITY,
    "VMM communication",
    vmm_com_handler,
  },
  {
    30,
    0,
    FT_SECURITY,
    "Security fault",
    security_fault_handler,
  },
  {
    31,
    0,
    FT_RESERVED,
    "Reserved 7",
    reserved_irq_handler,
  }
};

/*!
 * @brief: Prints a quick stackdump to the default kernel output
 */
int generate_traceback(registers_t* regs)
{
  const char* c_func;
  uint64_t c_offset;
  uint64_t sym_start;
  uint64_t ip, bp;
  uint8_t current_depth = 0,
          max_depth = 20;

  if (!regs)
    return -1;

  ip = regs->rip;
  bp = regs->rbp;

  printf("Traceback:\n");

  /*
   * TODO: check loaded drivers
   */
  while (ip && bp && current_depth < max_depth)
  {
    c_offset = 0;
    sym_start = 0;
    c_func = get_best_ksym_name(ip);

    if (!c_func)
      c_func = "Unknown";

    if (c_func)
      sym_start = get_ksym_address((char*)c_func);

    /* Check if we got sym_start */
    if (sym_start && ip > sym_start)
      c_offset = ip - sym_start;

    printf("(%d): Function name <%s+0x%llx> at (0x%llx)\n",
        current_depth,
        c_func,
        c_offset,
        ip);

    ip = *(uint64_t*)(bp + sizeof(uint64_t));
    bp = *(uint64_t*)bp;
    current_depth++;
  }

  return 0;
}

/*!
 * @brief: Simply copy over a static fault template
 */
int get_fault(uint32_t irq_num, aniva_fault_t* out)
{
  if (irq_num >= arrlen(_static_faults))
    return -1;

  *out = _static_faults[irq_num];

  /* ->type should never be zero (FT_UNIMPLEMENTED) */
  if (out->type == FT_UNIMPLEMENTED)
    return -2;

  return 0;
}

/*!
 * @brief: Print some (Perhaps) handy debug info
 *
 * TODO: figure out what else to put here
 */
static inline void _fault_print_debug(aniva_fault_t* fault, registers_t* ctx)
{
  printf(" *** System fault: \'%s\', at address (0x%llx) ***\n", fault->name, ctx->rip);
  printf("ISR number: %d\n", fault->irq_num);
}

/*!
 * @brief: 'Handle' a fatal fault by letting the user know what went wrong
 *
 * This function really should never return
 */
static void try_execute_fatal_fault_notify(aniva_fault_t* fault, registers_t* ctx)
{
  kernel_panic("TODO: let the user know about this fault any way we can!");
}

extern enum FAULT_RESULT try_handle_fault(aniva_fault_t* fault, registers_t* ctx)
{
  enum FAULT_RESULT result;

  if (!fault)
    return FR_FATAL;

  result = FR_FATAL;

  if ((fault->flags & FAULT_FLAG_PRINT_DEBUG) == FAULT_FLAG_PRINT_DEBUG)
    _fault_print_debug(fault, ctx);

  /* Should we do a traceback? */
  if ((fault->flags & FAULT_FLAG_TRACEBACK) == FAULT_FLAG_TRACEBACK)
    generate_traceback(ctx);

  /* Should we dump our entire system state? */
  if ((fault->flags & FAULT_FLAG_DEBUG_DUMP) == FAULT_FLAG_DEBUG_DUMP)
    kernel_panic("TODO: perform a complete system dump");

  if (fault->f_handle)
    /* Try to handle the fault */
    result = fault->f_handle(fault, ctx);

  /*
   * If trying to handle the fault resulted in a fatal result 
   * We should really make sure the user gets as much info as we can. This means 
   * we can either ask the current user environment really nicely if it wants to
   * stop what it's doing and help us out, or we can hijack the entire userenvironment
   * and try to launch our own kind of 'bluescreen'
   */
  if (result == FR_FATAL)
    try_execute_fatal_fault_notify(fault, ctx);

  /* Just in case the fatal fault notify failed or bailed or sm, we return here */
  return result;
}

static enum FAULT_RESULT div_by_zero_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("TODO (Div by zero): figure out what to do here");
}

static enum FAULT_RESULT debug_trap_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("TODO (Debug trap): figure out what to do here");
}

static enum FAULT_RESULT breakpoint_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("TODO (Breakpoint): figure out what to do here");
}

static enum FAULT_RESULT illegal_instr_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("TODO (Illegal instruction): figure out what to do here");
}

static enum FAULT_RESULT tss_err_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("TODO (Invalid TSS): figure out what to do here");
}

static enum FAULT_RESULT seg_not_present_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("TODO (Seg not present): figure out what to do here");
}

static enum FAULT_RESULT stack_seg_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("TODO (Stack seg): figure out what to do here");
}

static enum FAULT_RESULT reserved_irq_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("TODO (Reserved): figure out what to do here");
}

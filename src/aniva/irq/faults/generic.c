#include "faults.h"
#include "libk/bin/ksyms.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include <logging/log.h>

static enum FAULT_RESULT div_by_zero_handler(const aniva_fault_t* fault, registers_t* regs);

static const aniva_fault_t _static_faults[] = {
  {
    0,
    0,
    FT_DEVIDE_BY_ZERO,
    "Devide by zero",
    NULL,
    div_by_zero_handler,
  },
  {
    0,
    0,
    FT_DEBUG,
    "Debug trap",
    NULL,
    NULL,
  }
};

static enum FAULT_RESULT div_by_zero_handler(const aniva_fault_t* fault, registers_t* regs)
{
  kernel_panic("TODO: figure out what to do here");
}

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

  /* Try to assess */
  if (fault->f_assess)
    result = fault->f_assess(fault, ctx);

  if (result == FR_FATAL)
    return FR_FATAL;

  /* We can quickly fix this issue, skip debug */
  if (result == FR_SKIP_DEBUG)
    goto try_handle;

  if ((fault->flags & FAULT_FLAG_PRINT_DEBUG) == FAULT_FLAG_PRINT_DEBUG)
    _fault_print_debug(fault, ctx);

  /* Skip only the heavy part of the debug */
  if (result == FR_DO_PARTIAL_DEBUG)
    goto try_handle;

  /* Should we do a traceback? */
  if ((fault->flags & FAULT_FLAG_TRACEBACK) == FAULT_FLAG_TRACEBACK)
    generate_traceback(ctx);

  /* Should we dump our entire system state? */
  if ((fault->flags & FAULT_FLAG_DEBUG_DUMP) == FAULT_FLAG_DEBUG_DUMP)
    kernel_panic("TODO: perform a complete system dump");

try_handle:
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


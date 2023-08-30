#include "ctl.h"
#include "dev/debug/serial.h"
#include "entry/entry.h"
#include "intr/ctl/apic.h"
#include "intr/ctl/pic.h"
#include "libk/data/linkedlist.h"
#include "libk/string.h"
#include <mem/heap.h>
#include "mem/kmem_manager.h"
#include "sched/scheduler.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include "system/processor/processor.h"
#include "system/processor/processor_info.h"

static uint32_t lapic_address;

/* Most likely a DUAL PIC */
static int_controller_t* fallback;

static void mark_ctl_failure()
{
  g_system_info.sys_flags |= SYSFLAGS_NO_INTERRUPTS;
}

/*!
 * @brief Make sure all the vectors on the fallback controller are masked
 *
 * Nothing to add here...
 */
static void fallback_mask_all()
{
  if (!fallback)
    return;

  /* Mask all the vectors, NOTE: both PICs are capable of 8 irqs */
  for (uint32_t i = 0; i < 16; i++) {
    fallback->ictl_disable_vec(i);
  }
}

/*!
 * @brief Initialize a fallback dual PIC interrupt controller
 *
 * Nothing to add here...
 */
static void initialize_fallback_controller()
{
  fallback = get_pic();

  /* Enable the fallback controller */
  fallback->ictl_enable(fallback);

  fallback_mask_all();
}

/*!
 * @brief Remove the fallback controller
 *
 * If an advanced interrupt management chip can handle coverage of the entire interrupt range, we can 
 * (and should) remove and disable the fallback controller, since it can only fuck us over at that point
 */
static void remove_fallback_controller()
{
  /* Mask all the vectors, to be sure */
  fallback_mask_all();

  /* Disable the controller */
  fallback->ictl_disable(fallback);

  /* Remove the controller */
  fallback = nullptr;
}

static void reset_fallback_controller()
{
  fallback = get_pic();

  /* Enable, just in case */
  fallback->ictl_enable(fallback);

  /* Remove it again */
  remove_fallback_controller();
}

static inline bool has_fallback()
{
  return (fallback != nullptr);
}

void init_intr_ctl()
{
  int error;
  acpi_parser_t* parser;
  struct acpi_table_madt* madt;

  fallback = NULL;
  lapic_address = NULL;

  /* NOTE: this never fails */
  get_root_acpi_parser(&parser);

  madt = acpi_parser_find_table(parser, MADT_SIGNATURE, sizeof(struct acpi_table_madt));

  /* No MADT? That's just fatal lmao */
  if (!madt)
    goto no_valid_ctl;

  /* Completely disable the fallback controller early, so we can at least have that, if our machine does not have a APIC fsr lmao */
  if ((madt->flags & ACPI_MADT_PCAT_COMPAT) == ACPI_MADT_DUAL_PIC)
    reset_fallback_controller();

  /* Yay, we have a local apic for the bsp! */
  lapic_address = madt->address;

  error = init_bsp_apic(madt, lapic_address);

  /* If something went wrong while setting up the bsp lapic and IO apics, revert to fallback */
  if (error && (madt->flags & ACPI_MADT_PCAT_COMPAT) == ACPI_MADT_DUAL_PIC)
    initialize_fallback_controller();

  return;
no_valid_ctl:
  mark_ctl_failure();
}

/*!
 * @brief Get the active controller for a certain interrupt vector
 *
 * When the fallback controller is active and manages this vector, we'll use that.
 * otherwise, we'll look for APIC stuff
 */
int_controller_t *get_controller_for_int_number(uint16_t vector) 
{
  if (has_fallback() && vector < 16)
    return fallback;

  /* TODO: */
  return nullptr;
}

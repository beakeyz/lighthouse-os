#include "apic.h"
#include "intr/ctl/ctl.h"
#include <system/msr.h>

uint32_t apic_base_address;
uint64_t apic_hh_base_address;

/*!
 * @brief Initialize the BSP local apic
 *
 */
int init_bsp_apic(struct acpi_table_madt* table, uintptr_t base)
{
  /* For now, return error to enable dual PIC as a fallback */
  return -1;
}

int_controller_t apic_controller = {
};

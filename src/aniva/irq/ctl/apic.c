#include "apic.h"
#include "irq/ctl/ctl.h"
#include <system/msr.h>

uint32_t apic_base_address;
uint64_t apic_hh_base_address;

/*!
 * @brief Initialize the BSP local apic
 *
 */
int init_bsp_apic(acpi_tbl_madt_t* table, uintptr_t base)
{
  /* For now, return error to enable dual PIC as a fallback */
  return -1;
}

irq_chip_t apic_controller = {
};

#ifndef __ANIVA_ACPI_TABLES__
#define __ANIVA_ACPI_TABLES__

/* Include the ACPICA tables */
#include "acpica/actbl.h"

typedef ACPI_GENERIC_ADDRESS acpi_gen_addr_t;
typedef ACPI_RSDP_COMMON acpi_rsdp_t;
typedef ACPI_RSDP_EXTENSION acpi_xsdp_t;
/* Full r/xsdp */
typedef ACPI_TABLE_RSDP acpi_tbl_rsdp_t;

typedef ACPI_TABLE_DESC acpi_tbl_desc_t;
typedef ACPI_TABLE_RSDT acpi_tbl_rsdt_t;
typedef ACPI_TABLE_FACS apci_tbl_facs_t;
typedef ACPI_TABLE_FADT acpi_tbl_fadt_t;

typedef ACPI_TABLE_MCFG acpi_tbl_mcfg_t;

#endif // !__ANIVA_ACPI_TABLES__

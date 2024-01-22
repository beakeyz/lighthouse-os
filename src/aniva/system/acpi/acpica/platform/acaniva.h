/******************************************************************************
 *
 * Name: acaniva.h - OS specific defines, etc. for Aniva
 *
 *****************************************************************************/

#ifndef __ANIVA_ACPICA_AC__
#define __ANIVA_ACPICA_AC__

#include <sync/spinlock.h>
#include <mem/zalloc.h>
#include <libk/ctype.h>
#include <libk/string.h>
#include "system/asm_specifics.h"

/* Common ACPICA configuration */
#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_DO_WHILE_0
#define ACPI_IGNORE_PACKAGE_RESOLUTION_ERRORS

#define ACPI_USE_GPE_POLLING
#define ACPI_MUTEX_TYPE ACPI_OSL_MUTEX

/* Kernel specific ACPICA configuration */

#ifdef CONFIG_ACPI_REDUCED_HARDWARE_ONLY
#define ACPI_REDUCED_HARDWARE 1
#endif

#define ACPI_DEBUGGER
#define ACPI_DEBUG_OUTPUT

#ifdef CONFIG_ACPI_DEBUG
#define ACPI_MUTEX_DEBUG
#endif

#define ACPI_INIT_FUNCTION __init

/* Use a specific bugging default separate from ACPICA */
 
#undef ACPI_DEBUG_DEFAULT
#define ACPI_DEBUG_DEFAULT          (ACPI_LV_INFO | ACPI_LV_REPAIR)

/* Host-dependent types and defines for in-kernel ACPICA */

// NOTE: ANIVA; we assume aniva is only run on x86_64 machines!
#define ACPI_MACHINE_WIDTH          64
#define ACPI_USE_NATIVE_MATH64
//#define ACPI_EXPORT_SYMBOL(symbol)  EXPORT_SYMBOL(symbol);
#define ACPI_EXPORT_SYMBOL(symbol)
#define strtoul                     dirty_strtoul

#define ACPI_CACHE_T                struct zone_allocator
#define ACPI_SPINLOCK               spinlock_t *
#define ACPI_CPU_FLAGS              unsigned int

#define ACPI_UINTPTR_T              uintptr_t

#define ACPI_TO_INTEGER(p)          ((uintptr_t)(p))
#define ACPI_OFFSET(d, f)           GET_OFFSET(d, f)

#define ACPI_MSG_ERROR          "ACPI Error: "
#define ACPI_MSG_EXCEPTION      "ACPI Exception: "
#define ACPI_MSG_WARNING        "ACPI Warning: "
#define ACPI_MSG_INFO           "ACPI: "

#define ACPI_MSG_BIOS_ERROR     "ACPI BIOS Error (bug): "
#define ACPI_MSG_BIOS_WARNING   "ACPI BIOS Warning (bug): "

#define ACPI_STRUCT_INIT(field, value)  .field = value

#endif /* __ACLINUX_H__ */

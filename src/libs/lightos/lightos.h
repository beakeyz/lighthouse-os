#ifndef __ANIVA_LIBENV_SHAREDLIB__
#define __ANIVA_LIBENV_SHAREDLIB__

/*
 * Main header for the lightos userlib
 *
 * Supposed to be used by libraries alone
 */

#include "lightos/types.h"
#include "sys/types.h"

#define LIGHTENTRY_SECTION_NAME ".lightentry"
#define LIGHTEXIT_SECTION_NAME ".lightexit"

#define LIGHTENTRY static __attribute__((noinline, used, section(LIGHTENTRY_SECTION_NAME)))
#define LIGHTEXIT static __attribute__((noinline, used, section(LIGHTEXIT_SECTION_NAME)))

#define LEXPORT extern

#define LIGHTOS_SFLAGS_NOCMDLINE 0x00000001ULL
#define LIGHTOS_SFLAGS_NODEVICES 0x00000002ULL
#define LIGHTOS_SFLAGS_NOERRSTACK 0x00000002ULL

BOOL lightos_has_cmdline();
BOOL lightos_has_devices();
BOOL lightos_has_errstack();

error_t lightos_enable_errstack();
error_t lightos_disable_errstack();

#endif // !__ANIVA_LIBENV_SHAREDLIB__

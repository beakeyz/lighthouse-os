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

#define LEXPORT extern __attribute__((used))

#define LIGHTOS_SFLAGS_NOCMDLINE 0x00000001ULL
#define LIGHTOS_SFLAGS_NODEVICES 0x00000002ULL
#define LIGHTOS_SFLAGS_NOERRSTACK 0x00000002ULL

typedef int (*f_light_entry)(HANDLE phndl);
typedef int (*f_libinit)(HANDLE phndl);
typedef int (*f_libexit)(HANDLE phndl);

/*
 * App context struct for any lightos application
 * Passed into the %rdi register of any application that gets loaded
 */
typedef struct lightos_appctx {
    /* Number of libraries the lightrt needs to init */
    u32 nr_libentries;
    /* Handle to our owm process */
    HANDLE self;
    /* Entry function of this app */
    f_light_entry entry;
    /* Vector of init functions for the loaded libraries */
    f_libinit init_vec[];
} lightos_appctx_t;

extern BOOL lightos_has_cmdline();
extern BOOL lightos_has_devices();
extern BOOL lightos_has_errstack();

extern error_t lightos_enable_errstack();
extern error_t lightos_disable_errstack();

#endif // !__ANIVA_LIBENV_SHAREDLIB__

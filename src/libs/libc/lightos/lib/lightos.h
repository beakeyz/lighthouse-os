#ifndef __ANIVA_LIBENV_SHAREDLIB__
#define __ANIVA_LIBENV_SHAREDLIB__

/*
 * Main header for the lightos userlib
 *
 * Supposed to be used by libraries alone
 */

#define LIGHTENTRY_SECTION_NAME ".lightentry"
#define LIGHTEXIT_SECTION_NAME ".lightexit"

#define LIGHTENTRY static __attribute__((noinline, used, section(LIGHTENTRY_SECTION_NAME)))
#define LIGHTEXIT static __attribute__((noinline, used, section(LIGHTEXIT_SECTION_NAME)))

#define LEXPORT extern

extern int __init_lightos();

#endif // !__ANIVA_LIBENV_SHAREDLIB__

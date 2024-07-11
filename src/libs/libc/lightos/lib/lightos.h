#ifndef __ANIVA_LIBENV_SHAREDLIB__
#define __ANIVA_LIBENV_SHAREDLIB__

/*
 * Main header for the lightos userlib
 *
 *
 */

#define LIGHTENTRY_SECTION_NAME ".lightentry"
#define LIGHTEXIT_SECTION_NAME ".lightexit"

#define LIGHTENTRY static __attribute__((noinline, used, section(LIGHTENTRY_SECTION_NAME)))
#define LIGHTEXIT static __attribute__((noinline, used, section(LIGHTEXIT_SECTION_NAME)))

extern int __init_lightos();

#endif // !__ANIVA_LIBENV_SHAREDLIB__

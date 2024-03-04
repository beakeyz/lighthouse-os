#ifndef __ANIVA_LIBENV_SHAREDLIB__
#define __ANIVA_LIBENV_SHAREDLIB__

#define LIGHTENTRY static __attribute__((noinline, used, section(".lightentry")))
#define LIGHTEXIT static __attribute__((noinline, used, section(".lightexit")))

#endif // !__ANIVA_LIBENV_SHAREDLIB__

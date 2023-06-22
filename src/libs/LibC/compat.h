#ifndef __LIGHTENV_LIBC_COMPAT__
#define __LIGHTENV_LIBC_COMPAT__

#ifdef __cplusplus
#   define CPP_COMPAT_START extern "C" {
#   define CPP_COMPAT_END }
#else
#   define CPP_COMPAT_START
#   define CPP_COMPAT_END
#endif

#endif // !__LIGHTENV_LIBC_COMPAT__

#ifndef __LIGHTENV_STDARG__
#define __LIGHTENV_STDARG__

typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)

#define __va_copy(d, s) __builtin_va_copy(d, s)

#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif // !__LIGHTENV_STDARG__

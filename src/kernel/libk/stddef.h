#ifndef __STDDEF__
#define __STDDEF__

typedef signed              char    int8_t;
typedef unsigned            char    uint8_t;
typedef signed              short   int16_t;
typedef unsigned            short   uint16_t;
typedef signed              int     int32_t;
typedef unsigned            int     uint32_t;
typedef long long signed    int     int64_t;
typedef long long unsigned  int     uint64_t;
typedef unsigned            int     uint_t;

typedef char*                       va_list;
typedef uint64_t                    size_t;
typedef int64_t                     ssize_t;
typedef uint64_t                    uintptr_t;
typedef int                         bool;

#define asm __asm__
#define nullptr (void*)0

#define NULL 0

#define true 1
#define false 0

#define NORETURN __attribute__((noreturn))

// These are standard yoink material off the internet, go cry =)
#define __va_argsiz(t)	\
	(((sizeof(t) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

#define va_start(ap, pN)	\
	((ap) = ((va_list) __builtin_next_arg(pN)))

#define va_end(ap)	((void)0)

#define va_arg(ap, t)					\
	 (((ap) = (ap) + __va_argsiz(t)),		\
	  *((t*) (void*) ((ap) - __va_argsiz(t))))
  
#define Kib 1024
#define Mib Kib * Kib
#define Gib Mib * Kib
#define Tib Gib * Kib

#endif // !__STDDEF__

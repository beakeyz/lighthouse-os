#ifndef __ANIVA_STDDEF__
#define __ANIVA_STDDEF__

/*
 * NOTE: x86 allows inferred singing of integer values, meaning that the 'signed' keyword
 * does not NEED to be prefixed every time, but ARM does seem to require this to be the case.
 *
 * Not exactly sure though, so I'll have to verify...
 */

#include "stdint.h"

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

typedef __builtin_va_list va_list;

typedef void (*FuncPtr)();

/* Marks everything that comes from userspace and should thus be checked */
#ifndef __user
#define __user
#endif

#define FuncPtrWith(type, name) type (*name)()

#define asm __asm__
#define nullptr (void*)0
#define NULL 0
#define true 1
#define false 0

/* Support macros to help label owned and unowned pointers
   Use it not enforced, rather strongly recommended */
#define OWNED
#define UNOWNED

#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#define NORETURN __attribute__((noreturn))
#define NAKED __attribute__((naked))
#define SECTION(__sect) __attribute__((section(__sect)))
#define ALIGN(a) __attribute__((aligned(a)))
#define USED __attribute__((used))
#define UNUSED __attribute__((unused))
#define __init __attribute__((section(".__init")))

#define __mmio __attribute__((aligned(1)));

#define STATIC_CAST(type, value) ((type)(value))
#define DYNAMIC_CAST(type, value) (*(type*)&(value))

#define va_start(ap, pN) \
    __builtin_va_start(ap, pN)

#define va_end(ap) \
    __builtin_va_end(ap)

#define va_arg(ap, t) \
    __builtin_va_arg(ap, t)

#define arrlen(arr) (sizeof((arr)) / sizeof((arr)[0]))

#define Kib 1024
#define Mib Kib* Kib
#define Gib Mib* Kib
#define Tib Gib* Kib

#endif // !__STDDEF__

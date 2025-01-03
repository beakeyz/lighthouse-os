#ifndef __LIGHTENV_LIBC_TYPES__
#define __LIGHTENV_LIBC_TYPES__

/*
 * The standard LIBC types header
 * GCC requires us to have this header contain a bunch of symbols marked as
 * '-Required'
 * Besides these definitions, this header further contains all the types and definitions that
 * any userspace program under the aniva kernel might require. This includes, but is not limited
 * to: pointer types, integer types, floating point types, ect.
 * Some things may still need implementing (like floats atm) but all these things will
 * come when they are ready
 */

/* Standard posix int definitions */
#include <stdint.h>

/* -Required */
typedef long off_t;
typedef int pid_t;
typedef int time_t;

typedef u8 U8;
#define U16 u16
#define U32 u32
#define U64 u64
#define BOOL bool
#define FLOAT32 float
#define FLOAT64 double

#define VOID void

typedef void (*FuncPtr)();

#define asm __asm__
#define nullptr (void*)0

#define NULL 0

#define true 1
#define false 0

#define TRUE true
#define FALSE false

#define STATIC_CAST(type, value) ((type)(value))

/*
 * This form of casting is generally considered a bad practise,
 * since A. it isn't ABI compliant, and B. It is very janky and unsafe =/
 */
#define DYNAMIC_CAST(type, value) (*(type*)&(value))

/* Shortcut for getting the length of a static array */
#define arrlen(arr) (sizeof((arr)) / sizeof((arr)[0]))

/*
 * Definitions for accurate byte-sizes
 */
#define Kib 1024ULL
#define Mib Kib* Kib
#define Gib Mib* Kib
#define Tib Gib* Kib

#endif // !__LIGHTENV_LIBC_TYPES__

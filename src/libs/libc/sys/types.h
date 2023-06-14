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

#ifndef KERNEL

/* Standard posix int definitions */
#include <stdint.h>

/* -Required */
typedef int pid_t;

typedef uintptr_t process_result_t;

typedef float       FLOAT;

typedef uint8_t     byte_t;
typedef uint16_t    word_t;
typedef uint32_t    dword_t;
typedef uint64_t    qword_t;

#define BYTE        byte_t
#define WORD        word_t
#define DWORD       dword_t
#define QWORD       qword_t

typedef void (*FuncPtr)();

#define asm __asm__
#define nullptr (void*)0

#define NULL 0
#define true 1
#define false 0

#define STATIC_CAST(type, value) ((type)(value))

/*
 * This form of casting is generally considered a bad practise,
 * since A. it isn't ABI compliant, and B. It is very janky and unsafe =/
 */
#define DYNAMIC_CAST(type, value) (*(type*)&(value))

/*
 * Definitions for accurate byte-sizes
 */
#define Kib 1024
#define Mib Kib * Kib
#define Gib Mib * Kib
#define Tib Gib * Kib

/*
 * Few definitions for communicating status with the kernel
 */
#define SUCCESS         (0)
#define ERROR           (-1)

#endif // !KERNEL

#endif // !__LIGHTENV_LIBC_TYPES__

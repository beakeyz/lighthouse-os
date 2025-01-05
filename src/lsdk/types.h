#ifndef __LIGHTOS_LSDK_TYPES_H__
#define __LIGHTOS_LSDK_TYPES_H__

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef long long signed int int64_t;
typedef long long unsigned int uint64_t;

typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef uint64_t uintptr_t;

typedef uintptr_t vaddr_t;
typedef uintptr_t paddr_t;

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;

typedef uint8_t bool;

/* -Required */
typedef long off_t;
typedef int pid_t;
typedef int time_t;

#define U8 u8
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

#endif // !__LIGHTOS_LSDK_TYPES_H__

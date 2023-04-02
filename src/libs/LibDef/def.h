#ifndef __ANIVA_LIGHTENV_DEF__
#define __ANIVA_LIGHTENV_DEF__

typedef signed              char    int8_t;
typedef unsigned            char    uint8_t;
typedef signed              short   int16_t;
typedef unsigned            short   uint16_t;
typedef signed              int     int32_t;
typedef unsigned            int     uint32_t;
typedef long long signed    int     int64_t;
typedef long long unsigned  int     uint64_t;

typedef uint64_t                    size_t;
typedef int64_t                     ssize_t;
typedef uint64_t                    uintptr_t;
typedef int                         bool;

typedef uintptr_t                   vaddr_t;
typedef uintptr_t                   paddr_t;

typedef void (*FuncPtr)();

#define asm __asm__
#define nullptr (void*)0
#define NULL 0
#define true 1
#define false 0

#define STATIC_CAST(type, value) ((type)(value))
#define DYNAMIC_CAST(type, value) (*(type*)&(value))


#define Kib 1024
#define Mib Kib * Kib
#define Gib Mib * Kib
#define Tib Gib * Kib

#endif
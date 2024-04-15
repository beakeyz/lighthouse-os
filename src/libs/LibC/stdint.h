#ifndef __LIBENV_STDINT__
#define __LIBENV_STDINT__

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

typedef uintptr_t                   vaddr_t;
typedef uintptr_t                   paddr_t;

#endif // !__LIBENV_STDINT__

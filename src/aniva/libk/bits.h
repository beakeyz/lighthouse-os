#ifndef __ANIVA_LIBK_BITS__
#define __ANIVA_LIBK_BITS__
#include "stddef.h"
#include "asm.h"

#define bit_op(operation, num, addr)                              \
((__builtin_constant_p(nr) &&					                            \
    __builtin_constant_p((uintptr_t)(addr) != (uintptr_t)NULL) &&	\
    (uintptr_t)(addr) != (uintptr_t)NULL &&			                  \
    __builtin_constant_p(*(const unsigned long *)(addr))) ?     	\
   const##op(nr, addr) : op(nr, addr))

#define tst_bit(nr, address) bit_op(_test_bit, nr, address);
#define set_bit(nr, address) bit_op(__set_bit, nr, address);
#define clear_bit(nr, address) bit_op(__clear_bit, nr, address);

static ALWAYS_INLINE void x86_set_bit(long nr, volatile unsigned long address) {
  if (__builtin_constant_p(nr)) {
    asm volatile("orb %b1,%0"
    : "+m" (*(volatile char *) ((void *)(address) + ((nr)>>3)))
    : "iq" ((1 << ((nr) & 7)))
    : "memory");
  } else {
    asm volatile("bts %1,%0"
    : : "m" (*(volatile long *) (address)), "Ir" (nr) : "memory");
  }
}

static ALWAYS_INLINE bool constant_test_bit(long nr, const volatile unsigned long *address){
  return ((1UL << (nr & (31))) & (address[nr >> 5])) != 0;
}

static ALWAYS_INLINE bool variable_test_bit(long nr, const volatile unsigned long *address){
  bool val;

  asm volatile ("bt %2, %1"
  CC_SET(c)
  : CC_OUT(c) (val)
  : "m" (*(unsigned long *)address), "Ir" (nr) : "memory"
  );
  return val;
}

static ALWAYS_INLINE bool test_bit(long nr, const volatile unsigned long *address) {
  if (__builtin_constant_p(nr)) {
    return constant_test_bit(nr, address);
  }
  return variable_test_bit(nr, address);
}

static ALWAYS_INLINE uint64_t rol64(uint64_t value, uint32_t shift) {
  return (value << (shift & 63)) | (value >> ((-shift) & 63));
}

static ALWAYS_INLINE uint64_t ror64(uint64_t value, uint32_t shift) {
  return (value >> (shift & 63)) | (value << ((-shift) & 63));
}

static ALWAYS_INLINE uint64_t rol32(uint64_t value, uint32_t shift) {
  return (value << (shift & 31)) | (value >> ((-shift) & 31));
}

static ALWAYS_INLINE uint64_t ror32(uint64_t value, uint32_t shift) {
  return (value >> (shift & 31)) | (value << ((-shift) & 31));
}

static ALWAYS_INLINE uint64_t rol16(uint64_t value, uint32_t shift) {
  return (value << (shift & 15)) | (value >> ((-shift) & 15));
}

static ALWAYS_INLINE uint64_t ror16(uint64_t value, uint32_t shift) {
  return (value >> (shift & 15)) | (value << ((-shift) & 15));
}

static ALWAYS_INLINE uint64_t rol8(uint64_t value, uint32_t shift) {
  return (value << (shift & 8)) | (value >> ((-shift) & 8));
}

static ALWAYS_INLINE uint64_t ror8(uint64_t value, uint32_t shift) {
  return (value >> (shift & 8)) | (value << ((-shift) & 8));
}

#endif //__ANIVA_LIBK_BITS__




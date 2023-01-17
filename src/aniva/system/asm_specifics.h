#ifndef __ANIVA_ASM_SPECIFICS__
#define __ANIVA_ASM_SPECIFICS__
#include <libk/stddef.h>

uintptr_t read_cr0();
//uintptr_t read_cr1();
uintptr_t read_cr2();
uintptr_t read_cr3();
uintptr_t read_cr4();


void write_cr0(uintptr_t value);
void write_cr2(uintptr_t value);
void write_cr3(uintptr_t value);
void write_cr4(uintptr_t value);

ALWAYS_INLINE void write_gs(uint32_t offset, uintptr_t val) {
  asm volatile ("mov %[val], %%gs:%a[off]" ::[off] "ir"(offset), [val] "r"(val) : "memory");
}

ALWAYS_INLINE uintptr_t read_gs(uint32_t offset) {
  uintptr_t ret;
  asm volatile ("mov %%gs:%a[off], %[val]" :[val] "=r"(ret) :[off] "ir"(offset));
  return ret;
}

ALWAYS_INLINE void write_fs(uint32_t offset, uintptr_t val) {
  asm volatile ("mov %[val], %%fs:%a[off]" ::[off] "ir"(offset), [val] "r"(val) : "memory");
}

ALWAYS_INLINE uintptr_t read_fs(uint32_t offset) {
  uintptr_t ret;
  asm volatile ("mov %%fs:%a[off], %[val]" :[val] "=r"(ret) :[off] "ir"(offset));
  return ret;
}

ALWAYS_INLINE uintptr_t read_cs() {
  uintptr_t ret;
  asm volatile ("mov %%cs, %[cs]" : [cs] "=g"(ret));
  return ret;
}

ALWAYS_INLINE void __ltr(uint16_t sel) {
  asm volatile("ltr %0" :: "r"(sel));
}

#endif // !__ANIVA_ASM_SPECIFICS__

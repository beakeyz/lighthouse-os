#include "asm_specifics.h"

// FIXME: do these have to have volatile or not?

void write_cr0(uintptr_t value){
    asm volatile("mov %%rax, %%cr0" :: "a"(value));
}

uintptr_t read_cr0(){
  uintptr_t ret;
  asm volatile ("mov %%cr0, %%rax" : "=a"(ret));
  return ret;
}

// missing write_c2

uintptr_t read_cr2(){
  uintptr_t ret;
  asm volatile ("mov %%cr2, %%rax" : "=a"(ret));
  return ret;
}

void write_cr3(uintptr_t cr3){
  asm volatile("mov %%rax, %%cr3" :: "r"(cr3) : "memory");
}

uintptr_t read_cr3(){
  uintptr_t ret;
  asm volatile ("mov %%cr3, %%rax" : "=a"(ret));
  return ret;
}

void write_cr4(uintptr_t value){
  asm volatile ("mov %%rax, %%cr4" :: "a"(value));
}

uintptr_t read_cr4(){
  uintptr_t ret;
  asm volatile ("mov %%cr4, %%rax" : "=a"(ret));
  return ret;
}

uint64_t read_xcr0() {
  uint32_t eax, edx;
  asm volatile("xgetbv"
    : "=a"(eax), "=d"(edx)
    : "c"(0u));
  return eax + ((uint64_t)edx << 32);
}

void write_xcr0(uint64_t value) {
  uint32_t eax = value;
  uint32_t edx = value >> 32;
  asm volatile("xsetbv" ::"a"(eax), "d"(edx), "c"(0u));
}

void read_cpuid(uint32_t eax, uint32_t ecx,
                uint32_t* eax_out,
                uint32_t* ebx_out,
                uint32_t* ecx_out,
                uint32_t* edx_out) {
  asm volatile (
    "cpuid"
    :
    "=a"(*eax_out),
    "=b"(*ebx_out),
    "=c"(*ecx_out),
    "=d"(*edx_out)
    :
    "a"(eax),
    "c"(ecx)
    );
}
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

void read_cpuid(uint32_t eax, uint32_t ecx,
                uint32_t* eax_out,
                uint32_t* ebx_out,
                uint32_t* ecx_out,
                uint32_t* edx_out) {
  uint32_t eax_buffer;
  uint32_t ebx_buffer;
  uint32_t ecx_buffer;
  uint32_t edx_buffer;
  asm volatile (
    "cpuid"
    :
    "=a"(eax_buffer),
    "=b"(ebx_buffer),
    "=c"(ecx_buffer),
    "=d"(edx_buffer)
    :
    "a"(eax),
    "c"(ecx)
    );
  *eax_out = eax_buffer;
  *ebx_out = ebx_buffer;
  *ecx_out = ecx_buffer;
  *edx_out = edx_buffer;
}
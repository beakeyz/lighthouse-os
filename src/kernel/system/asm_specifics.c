#include "asm_specifics.h"

// FIXME: do these have to have volatile or not?

void write_cr0(uintptr_t value){
    asm volatile("mov %%rax, %%cr0" :: "a"(value));
}

void write_cr4(uintptr_t value){
    asm volatile ("mov %%rax, %%cr4" :: "a"(value));
}

uintptr_t read_cr0(){
    uintptr_t ret;
    asm volatile ("mov %%cr0, %%rax" : "=a"(ret));
    return ret;
}

uintptr_t read_cr2(){
    uintptr_t ret;
    asm volatile ("mov %%cr2, %%rax" : "=a"(ret));
    return ret;
}

uintptr_t read_cr3(){
    uintptr_t ret;
    asm volatile ("mov %%cr3, %%rax" : "=a"(ret));
    return ret;
}

void write_cr3(uintptr_t cr3){
    asm volatile("mov %%rax, %%cr3" :: "a"(cr3) : "memory");
}

uintptr_t read_cr4(){
    uintptr_t ret;
    asm volatile ("mov %%cr4, %%rax" : "=a"(ret));
    return ret;
}

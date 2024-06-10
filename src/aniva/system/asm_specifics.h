#ifndef __ANIVA_ASM_SPECIFICS__
#define __ANIVA_ASM_SPECIFICS__
#include <libk/stddef.h>

#define GET_OFFSET(type, field) __builtin_offsetof(type, field)

uintptr_t read_cr0();
// uintptr_t read_cr1();
uintptr_t read_cr2();
uintptr_t read_cr3();
uintptr_t read_cr4();

uint64_t read_xcr0();

void write_cr0(uintptr_t value);
void write_cr2(uintptr_t value);
void write_cr3(uintptr_t value);
void write_cr4(uintptr_t value);
void write_xcr0(uint64_t value);

void read_cpuid(uint32_t eax, uint32_t ecx, uint32_t* eax_out, uint32_t* ebx_out, uint32_t* ecx_out, uint32_t* edx_out);

ALWAYS_INLINE uintptr_t get_eflags()
{
    uintptr_t _flags = 0;
    asm volatile(
        "pushf \n"
        "pop %0\n"
        : "=rm"(_flags)::
            "memory");
    return _flags;
}

ALWAYS_INLINE void write_gs(uint32_t offset, uintptr_t val)
{
    asm volatile("mov %[val], %%gs:%a[off]" ::[off] "ir"(offset), [val] "r"(val) : "memory");
}

ALWAYS_INLINE uintptr_t read_gs(uint32_t offset)
{
    uintptr_t ret;
    asm volatile("mov %%gs:%a[off], %[val]" : [val] "=r"(ret) : [off] "ir"(offset));
    return ret;
}

ALWAYS_INLINE void write_fs(uint32_t offset, uintptr_t val)
{
    asm volatile("mov %[val], %%fs:%a[off]" ::[off] "ir"(offset), [val] "r"(val) : "memory");
}

ALWAYS_INLINE uintptr_t read_fs(uint32_t offset)
{
    uintptr_t ret;
    asm volatile("mov %%fs:%a[off], %[val]" : [val] "=r"(ret) : [off] "ir"(offset));
    return ret;
}

ALWAYS_INLINE uintptr_t read_cs()
{
    uintptr_t ret;
    asm volatile("mov %%cs, %[cs]" : [cs] "=g"(ret));
    return ret;
}

ALWAYS_INLINE void __ltr(uint16_t sel)
{
    asm volatile(
        "ltr %0" ::"r"(sel));
}

#endif // !__ANIVA_ASM_SPECIFICS__

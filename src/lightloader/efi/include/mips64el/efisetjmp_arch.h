#ifndef GNU_EFI_MIPS64EL_SETJMP_H
#define GNU_EFI_MIPS64EL_SETJMP_H

#define JMPBUF_ALIGN 8

typedef struct {
    /* GP regs */
    UINT64 RA;
    UINT64 SP;
    UINT64 FP;
    UINT64 GP;
    UINT64 S0;
    UINT64 S1;
    UINT64 S2;
    UINT64 S3;
    UINT64 S4;
    UINT64 S5;
    UINT64 S6;
    UINT64 S7;

#ifdef __mips_hard_float
    /* FP regs */
    UINT64 F24;
    UINT64 F25;
    UINT64 F26;
    UINT64 F27;
    UINT64 F28;
    UINT64 F29;
    UINT64 F30;
    UINT64 F31;
#endif
} ALIGN(JMPBUF_ALIGN) jmp_buf[1];

#endif /* GNU_EFI_MIPS64EL_SETJMP_H */

#ifndef GNU_EFI_X86_64_SETJMP_H
#define GNU_EFI_X86_64_SETJMP_H

#define JMPBUF_ALIGN 8

typedef struct {
    UINT64 Rbx;
    UINT64 Rsp;
    UINT64 Rbp;

    UINT64 Rdi;
    UINT64 Rsi;
    UINT64 R12;
    UINT64 R13;
    UINT64 R14;
    UINT64 R15;
    UINT64 Rip;
    UINT64 MxCsr;
    UINT8 XmmBuffer[160]; // XMM6 - XMM15
} ALIGN(JMPBUF_ALIGN) jmp_buf[1];

#endif /* GNU_EFI_X86_64_SETJMP_H */

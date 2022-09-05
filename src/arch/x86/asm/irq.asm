; This file will contain all the possible interrupts
; TODO:

[bits 64]

extern interupt_handler

; Some macros cuz I want to have a soul at the end of this
%macro IRQ_ENTRY 1
    global _irq%1    
    _irq%1:
        push 0
        push %1
        jmp intermediate_handler
%endmacro

%macro ISR_ENTRY_ERR 1
    global _isr%1
    _isr%1:
        push %1
        jmp intermediate_handler
%endmacro

%macro ISR_ENTRY 1
    global _isr%1
    _isr%1:
        push 0x00
        push %1
        jmp intermediate_handler
%endmacro


intermediate_handler:
    ; save regs
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp

    cld
    ; call c handler
    call interupt_handler

    ; restore regs
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; cleanup
    add rsp, 16
    iretq

; define all the irqs and isrs

IRQ_ENTRY 32
IRQ_ENTRY 33
IRQ_ENTRY 34
IRQ_ENTRY 35
IRQ_ENTRY 36
IRQ_ENTRY 37
IRQ_ENTRY 38
IRQ_ENTRY 39
IRQ_ENTRY 40
IRQ_ENTRY 41
IRQ_ENTRY 42
IRQ_ENTRY 43
IRQ_ENTRY 44
IRQ_ENTRY 45
IRQ_ENTRY 46
IRQ_ENTRY 47

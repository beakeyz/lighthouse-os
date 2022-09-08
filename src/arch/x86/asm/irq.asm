; This file will contain all the possible interrupts
; TODO:

[bits 64]
section .text

extern interupt_handler

global flush_idt
flush_idt:
    lidt  [rdi]
    ret

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

%macro push_all 0

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

%endmacro

%macro pop_all 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

%endmacro



intermediate_handler:
    ; save regs

    cld

    push_all

    mov rdi, rsp

    ; call c handler
    call interupt_handler

    mov rsp, rax
    ; restore regs
    pop_all
    ; cleanup
    add rsp, 16
    sti
    iretq

; define all the irqs and isrs

ISR_ENTRY 0
ISR_ENTRY 1
ISR_ENTRY 2
ISR_ENTRY 3
ISR_ENTRY 4
ISR_ENTRY 5
ISR_ENTRY 6
ISR_ENTRY 7
ISR_ENTRY_ERR   8
ISR_ENTRY 9
ISR_ENTRY_ERR   10
ISR_ENTRY_ERR   11
ISR_ENTRY_ERR   12
ISR_ENTRY_ERR   13
ISR_ENTRY_ERR   14
ISR_ENTRY 15
ISR_ENTRY 16
ISR_ENTRY_ERR   17
ISR_ENTRY 18
ISR_ENTRY 19
ISR_ENTRY 20
ISR_ENTRY 21
ISR_ENTRY 22
ISR_ENTRY 23
ISR_ENTRY 24
ISR_ENTRY 25
ISR_ENTRY 26
ISR_ENTRY 27
ISR_ENTRY 28
ISR_ENTRY 29
ISR_ENTRY_ERR   30
ISR_ENTRY 31
ISR_ENTRY 32
ISR_ENTRY 33
ISR_ENTRY 34
ISR_ENTRY 35
ISR_ENTRY 36
ISR_ENTRY 37
ISR_ENTRY 38
ISR_ENTRY 39
ISR_ENTRY 40
ISR_ENTRY 41
ISR_ENTRY 42
ISR_ENTRY 43
ISR_ENTRY 44
ISR_ENTRY 45
ISR_ENTRY 46
ISR_ENTRY 47

ISR_ENTRY 50

; TODO: user isrs
; ISR_ENTRY 127
; ISR_ENTRY 100

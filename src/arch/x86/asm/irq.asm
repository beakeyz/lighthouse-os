; This file will contain all the possible interrupts
; TODO:

[bits 64]
align 4

extern interupt_handler

global flush_idt
flush_idt:
    lidt  [rdi]
    ret

global load_standard_idtptr
load_standard_idtptr:
    [extern idt_ptr]
    lidt [idt_ptr]
    ret

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

global _isr0
global _isr1
global _isr2
global _isr3
global _isr4
global _isr5
global _isr6
global _isr7
global _isr8
global _isr9
global _isr10
global _isr11
global _isr12
global _isr13
global _isr14
global _isr15
global _isr16
global _isr17
global _isr18
global _isr19
global _isr20
global _isr21
global _isr22
global _isr23
global _isr24
global _isr25
global _isr26
global _isr27
global _isr28
global _isr29
global _isr30
global _isr31

_isr0:
	cli
	push byte 0	; A normal ISR stub that pops a dummy error code to keep a
				   ; uniform stack frame
	push byte 0
	jmp isr_common_stub

_isr1:
	cli
	push byte 0
	push byte 1
	jmp isr_common_stub
_isr2:
	cli
	push byte 0
	push byte 2
	jmp isr_common_stub
_isr3:
	cli
	push byte 0
	push byte 3
	jmp isr_common_stub
_isr4:
	cli
	push byte 0
	push byte 4
	jmp isr_common_stub
_isr5:
	cli
	push byte 0
	push byte 5
	jmp isr_common_stub
_isr6:
	cli
	xchg bx, bx
	push byte 0
	push byte 6
	jmp isr_common_stub
_isr7:
	cli
	push byte 0
	push byte 7
	jmp isr_common_stub
_isr8:
	cli
	push byte 8
	jmp isr_common_stub
_isr9:
	cli
	push byte 0
	push byte 9
	jmp isr_common_stub
_isr10:
	cli
	push byte 10
	jmp isr_common_stub
_isr11:
	cli
	push byte 11
	jmp isr_common_stub
_isr12:
	cli
	push byte 12
	jmp isr_common_stub
_isr13:
	cli
	push byte 13
	jmp isr_common_stub
_isr14:
	cli
	push byte 14
	jmp isr_common_stub
_isr15:
	cli
	push byte 0
	push byte 15
	jmp isr_common_stub
_isr16:
	cli
	push byte 0
	push byte 16
	jmp isr_common_stub
_isr17:
	cli
	push byte 0
	push byte 17
	jmp isr_common_stub
_isr18:
	cli
	push byte 0
	push byte 18
	jmp isr_common_stub
_isr19:
	cli
	push byte 0
	push byte 19
	jmp isr_common_stub
_isr20:
	cli
	push byte 0
	push byte 20
	jmp isr_common_stub
_isr21:
	cli
	push byte 0
	push byte 21
	jmp isr_common_stub
_isr22:
	cli
	push byte 0
	push byte 22
	jmp isr_common_stub
_isr23:
	cli
	push byte 0
	push byte 23
	jmp isr_common_stub
_isr24:
	cli
	push byte 0
	push byte 24
	jmp isr_common_stub
_isr25:
	cli
	push byte 0
	push byte 25
	jmp isr_common_stub
_isr26:
	cli
	push byte 0
	push byte 26
	jmp isr_common_stub
_isr27:
	cli
	push byte 0
	push byte 27
	jmp isr_common_stub
_isr28:
	cli
	push byte 0
	push byte 28
	jmp isr_common_stub
_isr29:
	cli
	push byte 0
	push byte 29
	jmp isr_common_stub
_isr30:
	cli
	push byte 0
	push byte 30
	jmp isr_common_stub
_isr31:
	cli
	push byte 0
	push byte 31
	jmp isr_common_stub


extern kpanic


isr_common_stub:


    push_all
    mov rdi, rsp

    ; call c handler
    cld

    call interupt_handler
    mov rax, 0x2f592f412f4b2f4f
    mov qword [0xb8000], rax

    mov rsp, rax
    ; restore regs
    pop_all
    ; cleanup
    add rsp, 16

    iret

; haha this is fun =)
global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15


irq0:
	cli
	push byte 0
	push byte 32
	jmp irq_common_stub
irq1:
	cli
	push byte 0
	push byte 33
	jmp irq_common_stub
irq2:
	cli
	push byte 0
	push byte 34
	jmp irq_common_stub
irq3:
	cli
	push byte 0
	push byte 35
	jmp irq_common_stub
irq4:
	cli	
	push byte 0
	push byte 36
	jmp irq_common_stub
irq5:
	cli
	push byte 0
	push byte 37
	jmp irq_common_stub
irq6:
	cli
	push byte 0
	push byte 38
	jmp irq_common_stub
irq7:
	cli
	push byte 0
	push byte 39
	jmp irq_common_stub
irq8:
	cli
	push byte 0
	push byte 40
	jmp irq_common_stub
irq9:
	cli
	push byte 0
	push byte 41
	jmp irq_common_stub
irq10:
	cli
	push byte 0
	push byte 42
	jmp irq_common_stub
irq11:
	cli
	push byte 0
	push byte 43
	jmp irq_common_stub
irq12:
	cli
	push byte 0
	push byte 44
	jmp irq_common_stub
irq13:
	cli
	push byte 0
	push byte 45
	jmp irq_common_stub
irq14:
	cli
	push byte 0
	push byte 46
	jmp irq_common_stub
irq15:
	cli
	push byte 0
	push byte 47
	jmp irq_common_stub

[extern] _irq_handler

irq_common_stub:

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

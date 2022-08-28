section .multiboot_header
header_start:
    dd 0x1badb002                ; magic number
    dd 0x7                       ; protected mode code
    dd -(0x1badb002 + 0x7) ; header length

    times 5 dd 0

    dd 0
    dd 1280
    dd 1024
    dd 32
header_end:

section .text
bits 32

global start
extern _start

start:
    mov word [0xb8000], 0x0248 ; H
    hlt
    
section .bss
stack_bottom:
    resb 4096 * 16
stack_top:

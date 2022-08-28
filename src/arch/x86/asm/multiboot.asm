section .multiboot_header
header_start:
    dd 0xe85250d6                ; magic number
    dd 0                         ; protected mode code
    dd header_end - header_start ; header length

    ; checksum
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

    ; required end tag
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:

extern _start

section .text
bits 32

start:
    mov word [0xb8000], 0x0248 ; H
    hlt
    
section .bss
stack_bottom:
    resb 4096 * 16
stack_top:

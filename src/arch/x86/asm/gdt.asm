section .text

global setup_gdt
setup_gdt:
  cli
  lgdt [gdtr]

global flush_gdt
flush_gdt:
    cli
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    pop rdi
    mov rax, 0x08
    push rax
    push rdi
    retfq

section .rodata

gdtr:
  dw gdt_end - gdt_start - 1
  dd gdt_start

gdt_start:
  dq 0

  dw 0xffff
  dw 0x0000
  db 0x00
  db 0x9A
  db 0b10101111
  db 0x00

  dw 0xffff
  dw 0x0000
  db 0x00
  db 0x92
  db 0b10101111
  db 0x00
gdt_end:

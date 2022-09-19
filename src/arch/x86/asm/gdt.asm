global gdt_flush

global load_gdt
load_gdt:
    lgdt [rdi]
    ; This should be a far jump
    jmp gdt_flush
    ret

gdt_flush:
	mov ax, 0x10
    mov ds, ax
    mov ds, ax
    mov es, ax
   	mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

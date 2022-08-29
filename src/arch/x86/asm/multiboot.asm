section .data

mbpointer:
  dd 0

section .multiboot_header
align 4
mboot:
	dd  0x1BADB002           ;Magic
	dd  0x7                  ;Flags (4KiB-aligned modules, memory info, framebuffer info)
	dd  -(0x1BADB002 + 0x7)  ;Checksum

	times 5 dd 0

    dd 0                      ;Graphics mode
	dd 1280                    ;Graphics width
	dd 1024                    ;Graphics height
	dd 32                     ;Graphics depth
mboot_end:


section .text
[bits 32]

global start
extern _start

start:
    cli
    cld

    xor ebx,ebx
    mov [mbpointer], ebx

    mov esp, stack_top

    push dword [mbpointer]
    call _start
    
    mov word [0xb8000], 0x0448
    hlt

section .bss
stack_bottom:
    resb 4096
stack_top:

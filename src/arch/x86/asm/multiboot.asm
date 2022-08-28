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


global start
extern _start

[bits 32]
section .text
align 4
start:
    cli
    cld
    
    call _start
    
    hlt

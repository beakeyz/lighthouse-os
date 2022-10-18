[bits 32]
section .multiboot_header

; for funzies mb1 here too =) (our kernel will die if the bl decides "ye imma use this one =D")
align 4
mboot:
	dd  0x1BADB002           ;Magic
	dd  0x7                  ;Flags (4KiB-aligned modules, memory info, framebuffer info)
	dd  -(0x1BADB002 + 0x7)  ;Checksum
	times 5 dd 0
	dd 0                      ;Graphics mode
	dd 640                    ;Graphics width
	dd 480                    ;Graphics height
	dd 32                     ;Graphics depth
mboot_end:

align 8
header_start:
    dd 0xe85250d6   ;magic_number
    dd 0            ;Protected mode
    dd header_end - header_start    ;Header length

    ;compute checksum
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))
    ;here ends the required part of the multiboot header
	;The following is the end tag, must be always present
    ;end tag

mb_fb_tag:
    dw 5 
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 32
mb_fb_tag_end:

    align 8
    dw 0    ;type
    dw 0    ;flags
    dd 8    ;size
header_end:

section .pre_text
[bits 32]
align 4

global start

global boot_pdpt
; We will still keep this map here (for now)
; I'll have to remember the fact that we practically have a dormant pd here
; so I can clean it up later...
global boot_pdpt_hh
global boot_pdt

extern _start

global stack_top

; We'll initially be in 32 bit mode(due to multiboot and damn grub), so we're going to check if we can transfer to 64 bit 
; So: check for paging, check for memory (?), check for errors
; if any step of the process goes wrong, we'll revert and boot in 32 bit mode anyway (if that's possible)
start:

    mov edi, ebx ; Address of multiboot structure
    mov esi, eax ; Magic number

    mov esp, stack_top
    ; TODO: cpuid and PAE checks ect.  
    
    mov eax, boot_pdpt
    or eax, 0x3
    mov [boot_pml4t], eax

    mov eax, boot_pdt
    or eax, 0x3
    mov [boot_pdpt], eax

    mov eax, boot_pdt
    mov ebx, 0x83
    mov ecx, 32

    .map_low_mem_entry:
        mov [eax], ebx
        add ebx, 0x200000
        add eax, 8

        dec ecx
        cmp ecx, 0
        jne .map_low_mem_entry

    ; set cr3
    mov eax, boot_pml4t
    mov cr3, eax

    ; enable PAE
    mov eax, cr4
    or eax, 0x60
    mov cr4, eax

    ; set the long mode bit
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; enable paging
    mov eax, cr0
    or eax, 1 << 31
    or eax, 1 << 16
    mov cr0, eax

    lgdt [gdtr]
    jmp (0x8):(long_start)


; start of 64 bit madness
[bits 64]
align 4
section .text

; start lol
long_start:
    
    cli 
    cld

    lgdt [gdtr]

    ; update seg registers with new gdt data
    mov ax, 0x10
    mov ss, ax  ; Stack segment selector
    mov ds, ax  ; data segment register
    mov es, ax  ; extra segment register
    mov fs, ax  ; extra segment register
    mov gs, ax  ; extra segment register

    mov rsp, stack_top
    
    call _start
    
    mov rax, 0x2f592f412f4b2f4f
    mov qword [0xb8000], rax

loopback:
    cli
    hlt
    jmp loopback

section .rodata

; gdt
align 8
gdtr:
    dw gdt_end - gdt_start - 1
    dq gdt_start
gdt_start:
    dq 0

    dw 0
    dw 0
    db 0
    db 0x9a
    db 0x20
    db 0

    dw 0xffff
    dw 0
    db 0
    db 0x92
    db 0
    db 0
gdt_end:


section .pts 

; Only for 64 bit
align 4096
boot_pml4t:
    times 4096 db 0
boot_pdpt:
    times 4096 db 0
boot_pdpt_hh:
    times 4096 db 0
boot_pdt:
    times 4096 db 0

section .bss
align 4096
end_of_mapped_memory:
    resq 1
; 16 kb stack

align 8
stack_bottom:
    resb 16385
stack_top:


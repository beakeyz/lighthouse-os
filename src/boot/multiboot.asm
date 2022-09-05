%define VIRT_BASE 0xffffffff80000000

section .multiboot_header
header_start:
    dd 0xe85250d6   ;magic_number
    dd 0            ;Protected mode
    dd header_end - header_start    ;Header length

    ;compute checksum
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

;framebuffer_tag_start:
;    dw  0x05    ;Type: framebuffer
;    dw  0x01    ;Optional tag
;    dd  framebuffer_tag_end - framebuffer_tag_start ;size
;    dd  0   ;Width - if 0 we let the bootloader decide
;    dd  0   ;Height - same as above
;    dd  0   ;Depth  - same as above
;framebuffer_tag_end:

    ;here ends the required part of the multiboot header
	;The following is the end tag, must be always present
    ;end tag
    dw 0    ;type
    dw 0    ;flags
    dd 8    ;size
header_end:

section .pre_text
[bits 32]

global start
extern _start

global stack_top

global end_of_mapped_memory

; We'll initially be in 32 bit mode(due to multiboot and damn grub), so we're going to check if we can transfer to 64 bit 
; So: check for paging, check for memory (?), check for errors
; if any step of the process goes wrong, we'll revert and boot in 32 bit mode anyway (if that's possible)
start:
    
    mov edi, ebx ; Address of multiboot structure
    mov esi, eax ; Magic number

    mov esp, stack_top - VIRT_BASE
    ; TODO: cpuid and PAE checks ect.  

    mov eax, boot_pdpt - VIRT_BASE
    or eax, 0b11
    mov dword [(boot_pml4t - VIRT_BASE) + 0], eax


    mov eax, boot_pdt - VIRT_BASE
    or eax, 0b11
    mov dword [(boot_pml4t - VIRT_BASE) + 511 * 8], eax
    
    mov eax, boot_pml4t - VIRT_BASE
    or eax, 0b11
    mov dword [(boot_pml4t - VIRT_BASE) + 510 * 8], eax

    mov eax, boot_pt - VIRT_BASE
    or eax, 0b11 
    mov dword [(boot_pdpt - VIRT_BASE) + 0], eax

    mov eax, boot_pt - VIRT_BASE
    or eax, 0b11
    mov dword [(boot_pdt - VIRT_BASE) + 510 * 8], eax

    mov ecx, 0  ; Loop counter

    .map_p2:
        mov eax, 0x200000
        mul ecx
        or eax, 0b10000011

        mov [(boot_pt - VIRT_BASE) + ecx * 8], eax

        inc ecx
        ; Check ecx agains the max amount of loops
        cmp ecx, 512
        jne .map_p2
    
    ; set cr3
    mov eax, (boot_pml4t - VIRT_BASE)
    mov cr3, eax

    ; enable PAE
    mov eax, cr4
    or eax, 1 << 5
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

    lgdt [gdt64.pointer_low - VIRT_BASE]
    jmp (0x8):(long_start - VIRT_BASE)

[bits 64]
section .text
long_start:
    
    ;mov qword rax, (511 << 39) | (510 << 30) | (511 << 21)
    ;mov qword[(end_of_mapped_memory - VIRT_BASE)], rax 

    ; update seg registers with new gdt data
    mov ax, 0x10
    mov ss, ax  ; Stack segment selector
    mov ds, ax  ; data segment register
    mov es, ax  ; extra segment register
    mov fs, ax  ; extra segment register
    mov gs, ax  ; extra segment register

    mov rsp, stack_top
    lgdt [gdt64.pointer]

    call _start
    
    mov rax, 0x2f592f412f4b2f4f
    mov qword [0xb8000], rax
    hlt

section .bss 

; Only for 64 bit
align 4096
boot_pml4t:
    resb 4096
boot_pdpt:
    resb 4096
boot_pdt:
    resb 4096
boot_pt:
    resb 4096

align 4096
end_of_mapped_memory:
    resq 1
; 16 kb stack
stack_bottom:
    resb 16385
stack_top:

section .rodata
gdt64:
    dq  0	;first entry = 0
    .code equ $ - gdt64
        dq (1 <<44) | (1 << 47) | (1 << 41) | (1 << 43) | (1 << 53)  ;second entry=code=8
    .data equ $ - gdt64
        dq (1 << 44) | (1 << 47) | (1 << 41)	;third entry = data = 10

.pointer:
    dw .pointer - gdt64 - 1
    dq gdt64
.pointer_low:
    dw .pointer - gdt64 - 1
    dq gdt64 - VIRT_BASE



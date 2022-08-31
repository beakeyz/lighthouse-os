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

section .pagetables

; Only for 64 bit
boot_pml4t:
    resb 4096
boot_pdpt:
    resb 4096
boot_pdt:
    resb 4096
boot_pt:
    resb 4096

section .pre_text
[bits 32]

global start
extern _start

global end_of_mapped_memory

; We'll initially be in 32 bit mode(due to multiboot and damn grub), so we're going to check if we can transfer to 64 bit 
; So: check for paging, check for memory (?), check for errors
; if any step of the process goes wrong, we'll revert and boot in 32 bit mode anyway (if that's possible)
start:
    cli
    cld

    xor ebx,ebx
    ; we already moved the multiboot data into the pointer so we dont have to worry about that anymore
    mov [mbpointer - VIRT_BASE], ebx

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
    mov esp, stack_top

    lgdt [gdt64.pointer_low - VIRT_BASE]
    jmp (0x8):(long_start - VIRT_BASE)

section .text

[bits 64]
long_start:
    ; stack (after we've enabled paging)
    mov ax, 0x10
    mov ss, ax  ; Stack segment selector
    mov ds, ax  ; data segment register
    mov es, ax  ; extra segment register
    mov fs, ax  ; extra segment register
    mov gs, ax  ; extra segment register

long_default:
    
    mov rsp, stack_top
    lgdt [gdt64.pointer]

    ;push qword [mbpointer]
    ;call _start
    
    mov qword [0xb8000], 0x2f592f412f4b2f4f
    hlt

section .bss

VIRT_BASE equ 0xffffffff80000000 

end_of_mapped_memory:
    resq 1

stack_bottom:
    resb 4096
stack_top:

section .rodata
gdt64:
    dq  0	;first entry = 0
    .code equ $ - gdt64
        ; set the following values:
        ; descriptor type: bit 44 has to be 1 for code and data segments
        ; present: bit 47 has to be  1 if the entry is valid
        ; read/write: bit 41 1 means that is readable
        ; executable: bit 43 it has to be 1 for code segments
        ; 64bit: bit 53 1 if this is a 64bit gdt
        dq (1 <<44) | (1 << 47) | (1 << 41) | (1 << 43) | (1 << 53)  ;second entry=code=8
    .data equ $ - gdt64
        dq (1 << 44) | (1 << 47) | (1 << 41)	;third entry = data = 10

.pointer:
    dw .pointer - gdt64 - 1
    dq gdt64
.pointer_low:
    dw .pointer - gdt64 - 1
    dq gdt64 - VIRT_BASE



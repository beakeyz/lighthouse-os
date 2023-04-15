section .multiboot_header

header_start:
  align 8
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
  dd mb_fb_tag_end - mb_fb_tag
  dd 0 
  dd 0
  dd 0 
mb_fb_tag_end:

  align 8
  dw 0    ;type
  dw 0    ;flags
  dd 8    ;size
header_end:

KERNEL_VIRT_BASE equ 0xFFFFffff80000000
KERNEL_PAGE_IDX equ (KERNEL_VIRT_BASE >> 21)

section .pre_text
[bits 32]
align 4

global start

global boot_pml4t
global boot_pdpt
global boot_pd0
global boot_pd0_p

extern _start

[global kstack_top]
[global kstack_bottom]

; We'll initially be in 32 bit mode(due to multiboot and damn grub), so we're going to check if we can transfer to 64 bit 
; So: check for paging, check for memory (?), check for errors
; if any step of the process goes wrong, we'll revert and boot in 32 bit mode anyway (if that's possible)
start:

  mov edi, ebx ; Address of multiboot structure
  mov esi, eax ; Magic number

  ; TODO: cpuid and PAE checks ect.  
  mov esp, kstack_top - KERNEL_VIRT_BASE

check_cpuid:
  pushfd
  pop eax
  mov ebx, eax
  xor eax, 0x200000
  push eax
  popfd
  nop
  pushfd
  pop eax
  cmp eax, ebx
  jnz cpuid_support

  ; No cpuid, hang

  mov eax, 'C'
  mov [0xb8000], eax 

  cld
  cli
spin:
  hlt
  jmp spin

cpuid_support:
  xor eax, eax
  xor ebx, ebx

  ; set cr3
  mov eax, boot_pml4t - KERNEL_VIRT_BASE
  mov cr3, eax

  ; map pdpt for identity
  mov eax, boot_pdpt - KERNEL_VIRT_BASE
  or eax, 0x3
  mov [(boot_pml4t - KERNEL_VIRT_BASE)], eax


  mov eax, boot_pd0 - KERNEL_VIRT_BASE
  or eax, 0x3
  mov [(boot_pdpt - KERNEL_VIRT_BASE)], eax

  ; map pdpt for high base
  mov eax, boot_hh_pdpt - KERNEL_VIRT_BASE
  or eax, 0x3
  mov [(boot_pml4t - KERNEL_VIRT_BASE) + 511 * 8], eax

  ; reroute back to the identity mapping
  mov eax, boot_pml4t - KERNEL_VIRT_BASE
  or eax, 0x3
  mov [(boot_pml4t - KERNEL_VIRT_BASE) + 510 * 8], eax
  
  ; map p2 into our p3 for the higher half aswell
  mov eax, boot_pd0 - KERNEL_VIRT_BASE
  or eax, 0x3
  mov [(boot_hh_pdpt - KERNEL_VIRT_BASE) + 510 * 8], eax

  mov eax, 0 
  mov ebx, boot_pd0_p - KERNEL_VIRT_BASE
  mov ecx, 32

  .fill_directory:
    or ebx, 0x3
    mov dword[(boot_pd0 - KERNEL_VIRT_BASE) + eax], ebx
    add ebx, 0x1000
    add eax, 8

    dec ecx
    cmp ecx, 0
    jne .fill_directory

  mov ecx, 0x1000
  mov ebx, 0 
  mov eax, 0 

  ; We are mapping a kernel with a range of 2 Mib, this does not scale for now
  ; NOTE: if our kernel gets too big (it won't) we're fucked
  .map_kernel:
    or ebx, 0x3
    mov [(boot_pd0_p - KERNEL_VIRT_BASE) + eax], ebx
    add ebx, 0x1000
    add eax, 8

    dec ecx
    cmp ecx, 0
    jne .map_kernel

  ; enable PAE + PSE
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

  lgdt [gdtr - KERNEL_VIRT_BASE]
  jmp (0x8):(long_start - KERNEL_VIRT_BASE)

; start of 64 bit madness
[bits 64]
section .text
align 4

; start lol
long_start:

  cli 
  cld

  ; update seg registers with new gdt data
  mov ax, 0x10
  mov ss, ax  ; Stack segment selector
  mov ds, ax  ; data segment register
  mov es, ax  ; extra segment register
  mov fs, ax  ; extra segment register
  mov gs, ax  ; extra segment register

  mov rsp, kstack_top - KERNEL_VIRT_BASE

  call _start
  
loopback:
  cld
  cli
  hlt
  jmp loopback

section .data

mb_ptr:
  dq 0

section .rodata
; gdt
gdtr:
  dw gdt_end - gdt_start - 1
  dq gdt_start - KERNEL_VIRT_BASE
gdt_start:
  dq 0

  dq (1 << 44) | (1 << 47) | (1 << 41) | (1 << 43) | (1 << 53)

  dq (1 << 44) | (1 << 47) | (1 << 41)

gdt_end:

section .pts 

; Only for 64 bit
align 4096
boot_pml4t:
  times 0x1000 db 0
boot_pdpt:
  times 0x1000 db 0
boot_pd0:
  times 0x1000 db 0
boot_pd0_p:
  times 0x80000 db 0

boot_hh_pdpt:
  times 0x1000 db 0

; hihi small stack =)
section .stack

kstack_bottom:
  times 16384 db 0
kstack_top:



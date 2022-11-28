section .multiboot_header

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
    dd 0 
    dd 0
    dd 0 
mb_fb_tag_end:

    align 8
    dw 6
    dw 0
    dd 8

    align 8
    dw 0    ;type
    dw 0    ;flags
    dd 8    ;size
header_end:

section .pre_text
[bits 32]
align 4096

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
  
  ;;; TODO: remove check
  cli

  hlt

  ; set cr3
  mov eax, boot_pml4t
  mov cr3, eax

  mov eax, boot_pdpt
  or eax, 0x3
  mov [boot_pml4t], eax

  mov eax, boot_pdt
  or eax, 0x3
  mov [boot_pdpt], eax

  mov eax, boot_pdt
  mov ebx, 0
  mov ecx, 32

  .map_low_mem_entry:
      or ebx, 0x83
      mov [eax], ebx
      add ebx, 0x200000
      add eax, 8

      dec ecx
      cmp ecx, 0
      jne .map_low_mem_entry

  ; enable PAE
  mov eax, cr4
  or eax, 32 
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

; gdt
align 8
gdtr:
  dw gdt_end - gdt_start
  dq gdt_start
gdt_start:
  dq 0

  dw 0xffff
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

section .stack
align 8
stack_bottom:
  times 16385 db 0
stack_top:


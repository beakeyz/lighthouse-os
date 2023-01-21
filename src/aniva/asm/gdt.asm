section .text

; TODO:
[global _flush_gdt]  
_flush_gdt:
  lgdt [rdi]

.reload_cs:
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov ss, ax

  mov rax, qword .far_tramp
  push qword 0x08
  push rax
  o64 retf
.far_tramp:
  ret

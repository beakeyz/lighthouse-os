section .text

; TODO:
[global _flush_gdt]  
_flush_gdt:
  lgdt [rdi]

.reloadSegs:
  push 0x08
  lea rax, [rel .reload_cs]
  push rax
  retfq
.reload_cs:
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  ret

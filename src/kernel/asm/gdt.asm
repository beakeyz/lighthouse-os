section .text

global setup_gdt
setup_gdt:
  cli
  lgdt [rdi]
  call flush_gdt
  ret

global flush_gdt
flush_gdt:
  cli
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  pop rdi
  mov rax, 0x08
  push rax
  push rdi
  retfq

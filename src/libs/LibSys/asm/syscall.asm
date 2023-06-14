[bits 64]

%macro __syscall_push_registers 0
  push rbx
  push rdx
  push rdi
  push rsi
  push r8
%endmacro

%macro __syscall_pop_registers 0
  pop r8
  pop rsi
  pop rdi
  pop rdx
  pop rbx
%endmacro

%macro __syscall_clear_registers 0
  xor rbx, rbx
  xor rdx, rdx
  xor rdi, rdi
  xor rsi, rsi
  xor r8, r8
%endmacro

[global syscall_0]
syscall_0:
  ; Move syscall ID into rax
  mov rdi, rax
  syscall

  retq

[global syscall_1]
syscall_1:
  ; Move syscall ID into rax
  mov rax, rdi
  
  __syscall_push_registers

  ; rsi is the System-V arg0
  ; rbx is our syscall arg0
  mov rbx, rsi

  syscall

  __syscall_pop_registers

  retq

[global syscall_2]
syscall_2:
  ; Move syscall ID into rax
  mov rax, rdi
  
  __syscall_push_registers

  ; rsi is the System-V arg0
  ; rbx is our syscall arg0
  mov rbx, rsi
  mov rdx, rdx

  syscall

  __syscall_pop_registers

  retq

[global syscall_3]
syscall_3:
  ; Move syscall ID into rax
  mov rax, rdi
  
  __syscall_push_registers

  ; rsi is the System-V arg0
  ; rbx is our syscall arg0
  mov rbx, rsi
  mov rdx, rdx
  mov rdi, rcx

  syscall

  __syscall_pop_registers

  retq

[global syscall_4]
syscall_4:
  ; Move syscall ID into rax
  mov rax, rdi
  
  __syscall_push_registers

  ; rsi is the System-V arg0
  ; rbx is our syscall arg0
  mov rbx, rsi
  mov rdx, rdx
  mov rdi, rcx
  mov rsi, r8

  syscall

  __syscall_pop_registers

  retq

[global syscall_5]
syscall_5:
  ; Move syscall ID into rax
  mov rax, rdi
  
  __syscall_push_registers

  ; rsi is the System-V arg0
  ; rbx is our syscall arg0
  mov rbx, rsi
  mov rdx, rdx
  mov rdi, rcx
  mov rsi, r8
  mov r8, r9

  syscall

  __syscall_pop_registers

  retq

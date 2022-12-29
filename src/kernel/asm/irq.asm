; This file will contain all the possible interrupts
; TODO:
bits 64
align 8

extern interrupt_handler

global flush_idt
flush_idt:
  lidt  [rdi]
  ret

%macro irq 1
[global irq_%1]
irq_%1:

  push 0x00
  push %1 ; In this case the error code is already present on the stack
  save_context

  cld

  mov rdi, rsp
  call interrupt_handler
  mov rsp, rax

  restore_context
  add rsp, 16
  iretq
%endmacro

%macro interrupt_service_routine 1
[global interrupt_service_routine_%1]
interrupt_service_routine_%1:
  push 0x00
  push %1 ; pushing the interrupt number for easier identification by the handler
  
  save_context

  cld

  mov rdi, rsp
  call interrupt_handler
  mov rsp, rax

  restore_context

  add rsp, 16
  
  iretq

%endmacro

%macro interrupt_service_routine_error_code 1
[global interrupt_service_routine_error_code_%1]
interrupt_service_routine_error_code_%1:

  push %1 ; In this case the error code is already present on the stack
  save_context

  cld

  mov rdi, rsp
  call interrupt_handler
  mov rsp, rax

  restore_context
  add rsp, 16
  iretq
%endmacro

%macro save_context 0
  push rax
  push rbx
  push rcx
  push rdx
  push rsi
  push rdi
  push rbp
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15
%endmacro

%macro restore_context 0
  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rbp
  pop rdi
  pop rsi
  pop rdx
  pop rcx
  pop rbx
  pop rax
%endmacro

interrupt_service_routine 0
interrupt_service_routine 1
interrupt_service_routine 2
interrupt_service_routine 3
interrupt_service_routine 4
interrupt_service_routine 5
interrupt_service_routine 6
interrupt_service_routine 7
interrupt_service_routine_error_code 8
interrupt_service_routine 9
interrupt_service_routine_error_code 10
interrupt_service_routine_error_code 11
interrupt_service_routine_error_code 12
interrupt_service_routine_error_code 13
interrupt_service_routine_error_code 14
interrupt_service_routine 15
interrupt_service_routine 16
interrupt_service_routine_error_code 17
interrupt_service_routine 18
irq 32
irq 33
irq 34
irq 35
irq 36
irq 37
irq 38
irq 39
irq 40
irq 41
irq 42
irq 43
irq 44
irq 45
irq 46
irq 47

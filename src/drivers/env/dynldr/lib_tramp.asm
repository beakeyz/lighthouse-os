[section .text]

[global __lib_trampoline]

align 4096
__lib_trampoline: ; RDI: actual lib function

  cmp rdi, 0
  je __exit_with_error

  call rdi

  ; At this point we want to preserve RAX
  jmp __do_exit_syscall

  ; When RDI is invalid, we land here
__exit_with_error:
  mov rax, -1
  
__do_exit_syscall:
  ; NOTE: preserve the return value inside RAX and call exit
  mov rbx, 0
  syscall

times (4096 - ($ - __lib_trampoline)) db 0

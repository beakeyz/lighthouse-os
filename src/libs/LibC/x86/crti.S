
.global _init
.global _fini

.section .init

_init:
  push %rbp
  movq %rsp, %rbp 

.section .fini

_fini:
  push %rbp
  movq %rsp, %rbp 

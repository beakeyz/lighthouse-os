.section .text
.global _start 

.extern main
.extern lightapp_startup

/*
 * Epic entrypoint for our userspace apps
 * 
 * This is only included when it's statically built
 */
_start:
  movq $main, %rdi
  andq $0xFFFFFFFFFFFFFFF0, %rsp

  call lightapp_startup

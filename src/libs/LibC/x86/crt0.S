.section .text
.globl _start 
.type _start, @function

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

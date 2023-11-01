.section .text
.global _start 

.extern main
.extern lightapp_startup

// Epic entrypoint for our userspace apps
_start:
  movq $main, %rdi
  andq $0xFFFFFFFFFFFFFFF0, %rsp

  call lightapp_startup

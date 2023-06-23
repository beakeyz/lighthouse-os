
.global __lightapp_startup

.extern main
.extern lightapp_startup

__lightapp_startup:

  movq $main, %rdi
  andq $0xFFFFFFFFFFFFFFF0, %rsp

  call lightapp_startup


.global __lightapp_startup

.extern Main
.extern lightapp_startup

__lightapp_startup:

  movq $Main, %rdi
  andq $0xFFFFFFFFFFFFFFF0, %rsp

  call lightapp_startup

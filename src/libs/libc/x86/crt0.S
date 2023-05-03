
.global __lightapp_startup

.extern Main

__lightapp_startup:
  cli
  hlt
  movq $Main, %rdi
  call lightapp_startup

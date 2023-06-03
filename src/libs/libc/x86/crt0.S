
.global __lightapp_startup

.extern Main
.extern lightapp_startup

__lightapp_startup:
  jmp __lightapp_startup
  movq $Main, %rdi
  call lightapp_startup

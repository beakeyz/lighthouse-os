
.global __lightapp_startup

.extern Main

__lightapp_startup:
  movq $Main, %rsi
  call lightapp_startup

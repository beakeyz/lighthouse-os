#include "system.h"

static syscall_result_t syscall_0(syscall_id_t id)
{

}

static syscall_result_t syscall_1(syscall_id_t id, uintptr_t arg0)
{

}

static syscall_result_t syscall_2(syscall_id_t id, uintptr_t arg0, uintptr_t arg1)
{

}

static syscall_result_t syscall_3(syscall_id_t id, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2)
{

}

static syscall_result_t syscall_4(syscall_id_t id, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
{

}

static syscall_result_t syscall_5(syscall_id_t id, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{

}

static syscall_result_t syscall_6(syscall_id_t id, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5)
{

}

syscall_result_t syscall_x(
  syscall_id_t id,
  size_t argc,
  uintptr_t arg0,
  uintptr_t arg1,
  uintptr_t arg2,
  uintptr_t arg3,
  uintptr_t arg4,
  uintptr_t arg5
)
{
  syscall_result_t __result;
  syscall_id_t __id = id;

  switch (argc) {
    case SYS_0ARG:
      __result = syscall_0(id);
      break;
    case SYS_1ARG:
      __result = syscall_1(id, arg0);
      break;
    case SYS_2ARG:
      __result = syscall_2(id, arg0, arg1);
      break;
    case SYS_3ARG:
      __result = syscall_3(id, arg0, arg1, arg2);
      break;
    case SYS_4ARG:
      __result = syscall_4(id, arg0, arg1, arg2, arg3);
      break;
    case SYS_5ARG:
      __result = syscall_5(id, arg0, arg1, arg2, arg3, arg4);
      break;
    case SYS_6ARG:
      __result = syscall_6(id, arg0, arg1, arg2, arg3, arg4, arg5);
      break;
  }

  return __result;
}

#include "system.h"
#include "syscall.h"

syscall_result_t syscall_x(
  syscall_id_t id,
  size_t argc,
  uintptr_t arg0,
  uintptr_t arg1,
  uintptr_t arg2,
  uintptr_t arg3,
  uintptr_t arg4
)
{
  syscall_result_t __result;
  syscall_id_t __id;

  if (argc > SYS_MAXARGS)
    return SYS_ERR;

  __id = id;

  switch (argc) {
    case SYS_0ARG:
      __result = syscall_0(__id);
      break;
    case SYS_1ARG:
      __result = syscall_1(__id, arg0);
      break;
    case SYS_2ARG:
      __result = syscall_2(__id, arg0, arg1);
      break;
    case SYS_3ARG:
      __result = syscall_3(__id, arg0, arg1, arg2);
      break;
    case SYS_4ARG:
      __result = syscall_4(__id, arg0, arg1, arg2, arg3);
      break;
    case SYS_5ARG:
      __result = syscall_5(__id, arg0, arg1, arg2, arg3, arg4);
      break;
    default:
      return SYS_ERR;
  }

  return __result;
}

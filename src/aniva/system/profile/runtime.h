#ifndef __ANIVA_SYSTEM_PROFILE_RUNTIME__
#define __ANIVA_SYSTEM_PROFILE_RUNTIME__

#include <libk/stddef.h>

#define RUNTIME_VARNAME_PROC_COUNT "PROC_COUNT"
#define RUNTIME_VARPATH_PROC_COUNT "Runtime/PROC_COUNT"

extern uint32_t runtime_get_proccount();
extern void runtime_add_proccount();
extern void runtime_remove_proccount();

#endif // !__ANIVA_SYSTEM_PROFILE_RUNTIME__

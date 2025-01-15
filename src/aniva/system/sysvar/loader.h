#ifndef __ANIVA_SYSVAR_LOADER__
#define __ANIVA_SYSVAR_LOADER__

#include "oss/object.h"
#include "system/profile/attr.h"
#include <libk/stddef.h>

struct file;

int sysvarldr_load_variables(oss_object_t* obj, enum PROFILE_TYPE ptype, struct file* file);
int sysvarldr_save_variables(oss_object_t* obj, enum PROFILE_TYPE ptype, struct file* file);

#endif // !__ANIVA_SYSVAR_LOADER__

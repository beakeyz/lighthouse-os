#ifndef __ANIVA_SYSVAR_LOADER__
#define __ANIVA_SYSVAR_LOADER__

#include "system/profile/attr.h"
#include <libk/stddef.h>

struct file;
struct oss_node;

int sysvarldr_load_variables(struct oss_node* node, enum PROFILE_TYPE ptype, struct file* file);
int sysvarldr_save_variables(struct oss_node* node, enum PROFILE_TYPE ptype, struct file* file);

#endif // !__ANIVA_SYSVAR_LOADER__

#ifndef __ANIVA_SYSVAR_LOADER__
#define __ANIVA_SYSVAR_LOADER__

#include <libk/stddef.h>

struct file;
struct oss_node;

int sysvarldr_load_variables(struct oss_node* node, uint16_t priv_lvl, struct file* file);

#endif // !__ANIVA_SYSVAR_LOADER__

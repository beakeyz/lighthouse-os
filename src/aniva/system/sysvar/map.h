#ifndef __ANIVA_SYSVAR_MAP__
#define __ANIVA_SYSVAR_MAP__

#include "lightos/proc/var_types.h"
#include <libk/stddef.h>

struct sysvar;
struct oss_node;

/*
 * Variables are mapped using the oss. They are either attached to OSS_PROFILE_NODE and OSS_PROC_ENV_NODE nodes.
 */

bool oss_node_can_contain_sysvar(struct oss_node* node);

struct sysvar* sysvar_get(struct oss_node* node, const char* key);
int sysvar_attach(struct oss_node* node, const char* key, uint16_t priv_lvl, enum SYSVAR_TYPE type, uint8_t flags, uintptr_t value);
int sysvar_detach(struct oss_node* node, const char* key, struct sysvar** var);

#endif // !__ANIVA_SYSVAR_MAP__

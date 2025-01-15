#ifndef __ANIVA_SYSVAR_MAP__
#define __ANIVA_SYSVAR_MAP__

#include "lightos/api/sysvar.h"
#include "system/profile/attr.h"
#include <libk/stddef.h>

struct sysvar;
struct oss_object;

/*
 * Variables are mapped using the oss.
 */

bool oss_object_can_contain_sysvar(struct oss_object* node);

struct sysvar* sysvar_get(struct oss_object* object, const char* key);
int sysvar_dump(struct oss_object* object, struct sysvar*** barr, size_t* bsize);
int sysvar_attach(struct oss_object* object, struct sysvar* var);
int sysvar_attach_ex(struct oss_object* object, const char* key, enum PROFILE_TYPE ptype, enum SYSVAR_TYPE type, uint8_t flags, void* value, size_t bsize);
int sysvar_detach(struct oss_object* object, const char* key, struct sysvar** var);

#endif // !__ANIVA_SYSVAR_MAP__

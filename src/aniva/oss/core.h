#ifndef __ANIVA_OSS_CORE__
#define __ANIVA_OSS_CORE__

/*
 * Core include file for the aniva OSS
 *
 * Oss is a graph-based system to store objects and their relationships to one
 * another.
 */

#include "lightos/types.h"

struct oss_object;

void init_oss();
void oss_test();

error_t oss_open_root_object(const char* path, struct oss_object** pobj);
error_t oss_open_object(const char* path, struct oss_object** pobj);
error_t oss_open_object_from(const char* path, struct oss_object* rel, struct oss_object** pobj);

error_t oss_connect_root_object(struct oss_object* object);

/* Tries to remove an object from the oss graph */
error_t oss_try_remove_object(struct oss_object* obj);

/* Gets the last oss_path entry pretty much lol */
const char* oss_get_endpoint_key(const char* path);

#endif // !__ANIVA_OSS_CORE__

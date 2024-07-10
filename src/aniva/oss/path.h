#ifndef __ANIVA_OSS_PATH_UTILS__
#define __ANIVA_OSS_PATH_UTILS__

#include "libk/stddef.h"

#define _OSS_PATH_SLASH '/'
#define _OSS_PATH_SKIP '%'

typedef struct oss_path {
    char** subpath_vec;
    u32 n_subpath;
} oss_path_t;

int oss_parse_path(const char* path, oss_path_t* p_path);
int oss_parse_path_ex(const char* path, oss_path_t* p_path, char seperator);
int oss_get_relative_path(const char* path, u32 subpath_idx, const char** p_relpath);
int oss_destroy_path(oss_path_t* path);

#endif // !__ANIVA_OSS_PATH_UTILS__

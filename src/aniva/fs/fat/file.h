#ifndef __ANIVA_FS_FATFILE__
#define __ANIVA_FS_FATFILE__

#include "fs/fat/core.h"
#include "fs/file.h"

typedef struct fat_file {
  file_t* parent;

  uint32_t* clusterchain_buffer;
  size_t clusters_num;

  uint32_t direntry_cluster;
  uint32_t direntry_offset;
  uint32_t clusterchain_offset;
} fat_file_t;

file_t* create_fat_file(fat_fs_info_t* info, uint32_t flags, const char* path);
void destroy_fat_file(fat_file_t* file);

size_t get_fat_file_size(fat_file_t* file); 

#endif // !__ANIVA_FS_FATFILE__

#ifndef __ANIVA_FS_FATFILE__
#define __ANIVA_FS_FATFILE__

#include "fs/dir.h"
#include "fs/fat/core.h"
#include "fs/file.h"

enum FAT_FILE_TYPE {
    FFILE_TYPE_FILE = 0,
    FFILE_TYPE_DIR,
};

typedef struct fat_file {
    union {
        file_t* file_parent;
        dir_t* dir_parent;
        void* parent;
    };

    /*
     * Cached array of directory entries, if this file is a directory
     * This should get reallocated every time the directory content changes
     */
    fat_dir_entry_t* dir_entries;
    uint32_t* clusterchain_buffer;
    uint32_t clusters_num;
    uint32_t n_direntries;

    uint32_t direntry_cluster;
    uint32_t direntry_offset;
    uint32_t clusterchain_offset;

    enum FAT_FILE_TYPE type;
} fat_file_t;

file_t* create_fat_file(fat_fs_info_t* info, uint32_t flags, const char* path);
dir_t* create_fat_dir(fat_fs_info_t* info, uint32_t flags, const char* path);
void destroy_fat_file(fat_file_t* file);

size_t get_fat_file_size(fat_file_t* file);

kerror_t fat_file_update_dir_entries(fat_file_t* file);

dir_ops_t* get_fat_dir_ops();

#endif // !__ANIVA_FS_FATFILE__

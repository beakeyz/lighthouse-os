#ifndef __ANIVA_FAT_FILE__
#define __ANIVA_FAT_FILE__

struct fat_dir_entry;

typedef struct fat_file {

  struct fat_dir_entry* m_dir_entry;
} fat_file_t;

#endif // !__ANIVA_FAT_FILE__

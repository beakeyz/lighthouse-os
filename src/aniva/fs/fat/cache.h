#ifndef __ANIVA_FAT_CACHE__
#define __ANIVA_FAT_CACHE__

/*
 * This is the FAT cache housekeeping file for aniva
 */

#include "fs/vnode.h"
#include "libk/flow/error.h"

void init_fat_cache(void);

ErrorOrPtr create_fat_info(vnode_t* node);
void destroy_fat_info(vnode_t* node);

#endif // !__ANIVA_FAT_CACHE__

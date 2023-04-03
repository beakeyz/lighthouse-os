#include <libk/stddef.h>
#include <libk/string.h>
#include <libk/error.h>
#include <fs/core.h>

/*
 * Example
 */

int ext2_mount(fs_type_t* type, const char* mountpoint, void* ptr) {

  return 0;
}

fs_type_t ext2_type = {
  .m_name = "ext2",
  .f_mount = ext2_mount,
  .m_flags = FST_REQ_DRIVER,
};

void init_ext2_fs() {

  /* TODO: caches? */

  register_filesystem(&ext2_type);
}

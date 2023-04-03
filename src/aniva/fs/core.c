#include "core.h"
#include "libk/error.h"
#include <sync/mutex.h>
#include <libk/stddef.h>
#include <libk/string.h>

static fs_type_t* fsystems;
static mutex_t* fsystems_lock;

static fs_type_t** find_fs_type(const char* name, size_t length) {
  
  fs_type_t** ret;

  for (ret = &fsystems; *ret; ret = &(*ret)->m_next) {
    /* Check if the names match and if ->m_name is a null-terminated string */
    if (memcmp((*ret)->m_name, name, length) && !(*ret)->m_name[length]) {
      break;
    }
  }

  return ret;
}

fs_type_t* get_fs_driver(fs_type_t* fs) {
  kernel_panic("TODO: implement get_fs_driver");
}

ErrorOrPtr register_filesystem(fs_type_t* fs) {

  fs_type_t** ptr;

  if (fs->m_next)
    return Error();

  mutex_lock(fsystems_lock);

  ptr = find_fs_type(fs->m_name, strlen(fs->m_name));

  if (*ptr) {
    return Error();
  } else {
    *ptr = fs;
  }

  mutex_unlock(fsystems_lock);

  return Success(0);
}

ErrorOrPtr unregister_filesystem(fs_type_t* fs) {
  kernel_panic("TODO: implement unregister_filesystem");
}

fs_type_t* get_fs_type(const char* name) {
  kernel_panic("TODO: implement get_fs_type");
}

void init_fs_core() {

  fsystems_lock = create_mutex(0);

}

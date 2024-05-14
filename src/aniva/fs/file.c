#include "file.h"
#include "mem/heap.h"
#include "mem/page_dir.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include <libk/flow/error.h>
#include <libk/string.h>

static int _file_close(file_t* file);
file_t f_kmap(file_t* file, page_dir_t* dir, size_t size, uint32_t custom_flags, uint32_t page_flags);

/*
 * The standard file read funciton will try to read from the files data 
 * buffer if that exsists, otherwise we will have to go to the device to read
 * from it
 */
int f_read(file_t* file, void* buffer, size_t* size, uintptr_t offset) {

  uintptr_t end_offset;
  uintptr_t read_offset;

  if (!buffer || !size)
    return -1;

  /* FIXME: we should still be able to read from files even if they have no buffer */
  if (!file || !file->m_buffer)
    return -1;

  end_offset = offset + *size;
  read_offset = (uintptr_t)file->m_buffer + offset;

  /* TODO: in this case, we could try to read from the device (filesystem) */
  if (offset > file->m_buffer_size) {
    *size = 0;
    return -1;
  }

  /* TODO: also in this case, we should read from the device (filesystem) */
  if (end_offset > file->m_buffer_size) {
    uintptr_t delta = end_offset - file->m_buffer_size;

    /* We'll just copy everything we can */
    *size -= delta;
  }

  memcpy(buffer, (void*)read_offset, *size);

  return 0;
}

int f_write(file_t* file, void* buffer, size_t* size, uintptr_t offset) {

  kernel_panic("Tried to write to a file! (TODO: implement)");

  return 0;
}

int f_resize(file_t* file, size_t new_size) {
  /*
   * TODO: if new_size > ->m_size then we need to enlarge the buffer
   * if new_size < ->m_size then we can simply shrink the size and 
   * free up the memory that that gives us
   */

  kernel_panic("TODO: implement f_resize");

  return 0;
}

/*
 * NOTE: This function may block for a while
 */
int generic_f_sync(file_t* file) {

  int result;
  oss_obj_t* object;
  oss_node_t* parent_node;

  if (!file)
    return -1;

  object = file->m_obj;

  if (!object)
    return -1;

  parent_node = object->parent;

  if (!parent_node || !parent_node->ops || !parent_node->ops->f_force_obj_sync)
    return -1;

  /* Just force the node to sync this object */
  result = parent_node->ops->f_force_obj_sync(file->m_obj);

  if (result) {
    kernel_panic("FIXME: Could not sync file! (Implement handler)");
  }

  return result;
}


file_ops_t generic_file_ops = {
  .f_close = nullptr,
  .f_sync = generic_f_sync,
  .f_read = f_read,
  .f_write = f_write,
  .f_kmap = f_kmap,
  .f_resize = f_resize,
};

/*
 * NOTE: mapping a file means the mappings get the same flags, both
 * in the kernel map, and in the new map
 */
file_t f_kmap(file_t* file, page_dir_t* dir, size_t size, uint32_t custom_flags, uint32_t page_flags) 
{
  kernel_panic("TODO: fix f_kmap");
}

/*!
 * @brief: Allocate memory for a file and initialize its variables 
 *
 * This also creates a vobject, which is responsible for managing the lifetime
 * of the files memory
 */
file_t* create_file(struct oss_node* parent, uint32_t flags, const char* path) 
{
  const char* name;
  file_t* ret;

  ret = kmalloc(sizeof(file_t));

  if (!ret)
    return nullptr;

  name = oss_get_objname(path);

  if (!name)
    goto exit_and_dealloc;

  ret->m_flags = flags;
  ret->m_obj = create_oss_obj(name);

  kfree((void*)name);

  if (!ret->m_obj)
    goto exit_and_dealloc;

  /*
   * Register this file to its child object 
   * When the object gets closed, destroy_file will be called, 
   * which will be responsible for calling its own private destroy callback 
   */
  oss_obj_register_child(ret->m_obj, ret, OSS_OBJ_TYPE_FILE, destroy_file);

  ret->m_ops = &generic_file_ops;

  ret->m_buffer = nullptr;
  ret->m_buffer_size = 0;

  return ret;

exit_and_dealloc:
  kfree(ret);

  return nullptr;
}

void file_set_ops(file_t* file, file_ops_t* ops)
{
  if (!file || !ops)
    return;

  file->m_ops = ops;
}

/*!
 * @brief: Get that file object off of the heap
 */
void destroy_file(file_t* file) 
{
  /* Try to close the file */
  (void)_file_close(file);

  kfree(file);
}

/*!
 * @brief: Read data from a file
 */
size_t file_read(file_t* file, void* buffer, size_t size, uintptr_t offset)
{
  kerror_t error;
  oss_obj_t* file_obj;

  if (!file || !file->m_ops || !file->m_ops->f_read)
    return 0;

  file_obj = file->m_obj;

  if (!file_obj)
    return 0;

  mutex_lock(file_obj->lock);

  error = file->m_ops->f_read(file, buffer, &size, offset);

  mutex_unlock(file_obj->lock);

  if (error)
    return 0;

  return size;
}

/*!
 * @brief: Write data into a file
 *
 * The filesystem may choose if it wants to use buffers that get synced by
 * ->f_sync or if they want to instantly write to disk here
 */
size_t file_write(file_t* file, void* buffer, size_t size, uintptr_t offset)
{
  kerror_t error;
  oss_obj_t* file_obj;

  if (!file || !file->m_ops || !file->m_ops->f_write)
    return 0;

  file_obj = file->m_obj;

  if (!file_obj)
    return 0;

  mutex_lock(file_obj->lock);

  error = file->m_ops->f_write(file, buffer, &size, offset);

  mutex_unlock(file_obj->lock);

  if (error)
    return 0;

  return size;
}

/*!
 * @brief: Sync local file buffers with disk
 */
int file_sync(file_t* file)
{
  int error;
  oss_obj_t* file_obj;

  if (!file || !file->m_ops || !file->m_ops->f_sync)
    return -1;

  file_obj = file->m_obj;

  if (!file_obj)
    return -2;

  mutex_lock(file_obj->lock);

  error = file->m_ops->f_sync(file);

  mutex_unlock(file_obj->lock);

  return error;
}

/*!
 * @brief: Call the files private close function
 *
 * This should never be called as a means to close a file on a vobj, but in stead
 * the vobject should be destroyed with an unref, which will initialize the vobjects destruction
 * uppon the depletion of the ref. This will automatically call any destruction functions 
 * given up by its children, including this in the case of a vobject with a file attached
 */
static int _file_close(file_t* file)
{
  int error;
  oss_obj_t* file_obj;

  if (!file || !file->m_ops || !file->m_ops->f_close)
    return -1;

  file_obj = file->m_obj;

  if (!file_obj)
    return -2;

  mutex_lock(file_obj->lock);

  /* NOTE: the external close function should never kill the file object, just the internal buffers used by the filesystem */
  error = file->m_ops->f_close(file);

  mutex_unlock(file_obj->lock);

  return error;
}

file_t* file_open(const char* path)
{
  int error;
  file_t* ret;
  oss_obj_t* obj;

  /*
   * File gets created by the filesystem driver
   */
  error = oss_resolve_obj(path, &obj);

  if (error || !obj)
    return nullptr;

  /* Try to recieve the file created by the fs */
  ret = oss_obj_get_file(obj);

  if (!ret)
    oss_obj_close(obj);

  return ret;
}

int file_close(file_t* file)
{
  if (!file)
    return -1;

  return oss_obj_close(file->m_obj);
}

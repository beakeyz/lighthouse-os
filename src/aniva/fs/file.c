#include "file.h"
#include "lightos/api/objects.h"
#include "mem/heap.h"
#include "mem/page_dir.h"
#include "oss/core.h"
#include "oss/object.h"
#include <libk/flow/error.h>
#include <libk/string.h>

static int _file_close(file_t* file);
file_t f_kmap(file_t* file, page_dir_t* dir, size_t size, uint32_t custom_flags, uint32_t page_flags);

/*
 * The standard file read funciton will try to read from the files data
 * buffer if that exsists, otherwise we will have to go to the device to read
 * from it
 */
ssize_t f_read(file_t* file, void* buffer, size_t size, uintptr_t offset)
{

    uintptr_t end_offset;
    uintptr_t read_offset;

    if (!buffer || !size)
        return -1;

    /* FIXME: we should still be able to read from files even if they have no buffer */
    if (!file || !file->m_buffer)
        return -1;

    end_offset = offset + size;
    read_offset = (uintptr_t)file->m_buffer + offset;

    /* TODO: in this case, we could try to read from the device (filesystem) */
    if (offset > file->m_buffer_size)
        return 0;

    /* TODO: also in this case, we should read from the device (filesystem) */
    if (end_offset > file->m_buffer_size) {
        uintptr_t delta = end_offset - file->m_buffer_size;

        /* We'll just copy everything we can */
        size -= delta;
    }

    memcpy(buffer, (void*)read_offset, size);

    return size;
}

ssize_t f_write(file_t* file, void* buffer, size_t size, uintptr_t offset)
{

    kernel_panic("Tried to write to a file! (TODO: implement)");

    return 0;
}

/*
 * NOTE: This function may block for a while
 */
int generic_f_sync(file_t* file)
{
    return 0;
}

file_ops_t generic_file_ops = {
    .f_close = nullptr,
    .f_sync = generic_f_sync,
    .f_read = f_read,
    .f_write = f_write,
    .f_kmap = f_kmap,
};

static int destroy_file(oss_object_t* object)
{
    file_t* file = get_file_from_object(object);

    if (!file)
        return -EINVAL;

    /* Try to close the file */
    (void)_file_close(file);

    /* Free the file memory */
    kfree(file);

    return 0;
}

static ssize_t __file_oss_read(oss_object_t* object, u64 offset, void* buffer, size_t size)
{
    file_t* file = get_file_from_object(object);

    if (!file)
        return -EINVAL;

    /* Call the internal file read function */
    return file_read(file, buffer, size, offset);
}

static ssize_t __file_oss_write(oss_object_t* object, u64 offset, void* buffer, size_t size)
{
    file_t* file = get_file_from_object(object);

    if (!file)
        return -EINVAL;

    /* Call the internal file read function */
    return file_write(file, buffer, size, offset);
}

static int __file_oss_flush(oss_object_t* object)
{
    file_t* file = get_file_from_object(object);

    if (!file)
        return -EINVAL;

    /* Call the internal file read function */
    return file_sync(file);
}

static oss_object_ops_t file_oss_ops = {
    .f_Destroy = destroy_file,
    .f_Read = __file_oss_read,
    .f_Write = __file_oss_write,
    .f_Flush = __file_oss_flush,
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
file_t* create_file(fs_root_object_t* fsroot, uint32_t flags, const char* key)
{
    file_t* ret;

    if (!key)
        return nullptr;

    ret = kmalloc(sizeof(file_t));

    if (!ret)
        return nullptr;

    /* Clear the file buffer */
    memset(ret, 0, sizeof(file_t));

    ret->m_flags = flags;
    ret->fsroot = fsroot;
    ret->m_obj = create_oss_object(key, NULL, OT_FILE, &file_oss_ops, ret);

    if (!ret->m_obj)
        goto exit_and_dealloc;

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
 * @brief: Read data from a file
 */
ssize_t file_read(file_t* file, void* buffer, size_t size, uintptr_t offset)
{
    ssize_t error_or_size;
    oss_object_t* file_obj;

    if (!file || !file->m_ops || !file->m_ops->f_read)
        return 0;

    file_obj = file->m_obj;

    if (!file_obj)
        return 0;

    mutex_lock(file_obj->lock);

    error_or_size = file->m_ops->f_read(file, buffer, size, offset);

    mutex_unlock(file_obj->lock);

    return error_or_size;
}

/*!
 * @brief: Write data into a file
 *
 * The filesystem may choose if it wants to use buffers that get synced by
 * ->f_sync or if they want to instantly write to disk here
 */
ssize_t file_write(file_t* file, void* buffer, size_t size, uintptr_t offset)
{
    ssize_t error_or_size;
    oss_object_t* file_obj;

    if (!file || !file->m_ops || !file->m_ops->f_write)
        return 0;

    file_obj = file->m_obj;

    if (!file_obj)
        return 0;

    mutex_lock(file_obj->lock);

    error_or_size = file->m_ops->f_write(file, buffer, size, offset);

    mutex_unlock(file_obj->lock);

    return error_or_size;
}

/*!
 * @brief: Sync local file buffers with disk
 */
int file_sync(file_t* file)
{
    int error;
    oss_object_t* file_obj;

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
    oss_object_t* file_obj;

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

file_t* file_open_from(oss_object_t* node, const char* path)
{
    int error;
    oss_object_t* obj;

    if (node)
        /*
         * File gets created by the filesystem driver
         */
        error = oss_open_object_from(path, node, &obj);
    else
        error = oss_open_object(path, &obj);

    if (error || !obj || obj->type != OT_FILE || !obj->private)
        return nullptr;

    return obj->private;
}

file_t* file_open(const char* path)
{
    return file_open_from(nullptr, path);
}

int file_close(file_t* file)
{
    if (!file)
        return -1;

    return oss_object_close(file->m_obj);
}

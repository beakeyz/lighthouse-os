#include "file.h"
#include "lightos/api/filesystem.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static File* __create_file(u32 flags, Object* object)
{
    File* ret;

    if (!object)
        return nullptr;

    ret = malloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    /* Set the files object */
    ret->object = object;

    /* Initialize file buffers */
    FileSetBuffers(ret, LIGHTOS_FILE_DEFAULT_RBSIZE, (flags & OF_READONLY) ? 0 : LIGHTOS_FILE_DEFAULT_WBSIZE);

    return ret;
}

File* CreateFile(const char* key, u32 flags)
{
    return __create_file(flags, CreateObject(key, flags, OT_FILE));
}

File* OpenFile(const char* path, u32 hndl_flags, enum HNDL_MODE mode)
{
    return OpenFileFrom(nullptr, path, hndl_flags, mode);
}

File* OpenFileFrom(Object* rel, const char* path, u32 hndl_flags, enum HNDL_MODE mode)
{
    Object* object;

    if (!path)
        return nullptr;

    object = OpenObjectFrom(rel, path, hndl_flags, OT_FILE, mode);

    if (!ObjectIsValid(object))
        return nullptr;

    if (object->type != OT_FILE)
        CloseObject(rel);

    return __create_file((hndl_flags & HF_W) ? NULL : OF_READONLY, object);
}

error_t CloseFile(File* file)
{
    error_t error;

    /* Flush the file buffer */
    FileFlush(file);

    /* First try to close the files object */
    error = CloseObject(file->object);

    /* We failed. Return it */
    if (error)
        return error;

    if (file->rd_buff)
        free(file->rd_buff);

    if (file->wr_buff)
        free(file->wr_buff);

    /* Free the file memory */
    free(file);

    return 0;
}

error_t FileSetBuffers(File* file, size_t r_bsize, size_t w_bsize)
{
    file->rd_bsize = r_bsize;
    file->wr_bsize = w_bsize;

    /* Free old buffers if we have those */
    if (file->rd_buff)
        free(file->rd_buff);

    /* Now allocate new buffers */
    if (r_bsize) {
        file->rd_buff = malloc(file->rd_bsize);
        memset(file->rd_buff, 0, r_bsize);
    }

    /* Set this guy to null, so we can allocate it on the first read */
    file->wr_buff = nullptr;

    return 0;
}

bool FileIsValid(File* file)
{
    return ObjectIsValid(file->object);
}

int __read_byte(File* file, u64 offset, uint8_t* buffer)
{
    if (!buffer || !file->rd_buff)
        return NULL;

    /* Check if this read is inside the currernt read buffer */
    if (offset >= file->rd_bstart && offset < (file->rd_bstart + file->rd_bsize))
        *buffer = file->rd_buff[offset - file->rd_bstart];
    else {
        /* This read is outside the current read buffer, pull in a new block */
        if (ObjectRead(file->object, offset, file->rd_buff, file->rd_bsize) < 0)
            return NULL;

        /* Set the new buffer start */
        file->rd_bstart = offset;

        /* Grab the first byte of the new buffer */
        *buffer = file->rd_buff[0];
    }

    return 1;
}

int __read_bytes(File* file, u64 offset, uint8_t* buffer, size_t size)
{
    int ret = 0;
    int result;

    for (uint64_t i = 0; i < size; i++) {
        result = __read_byte(file, offset + i, &buffer[i]);

        if (!result)
            break;

        ret++;
    }

    /* Return the amount of bytes read */
    return ret;
}

size_t FileRead(File* file, void* buffer, size_t bsize)
{
    size_t read_size;

    read_size = FileReadEx(file, file->rd_bhead, buffer, bsize, 1);

    /* Increase the read offset */
    file->rd_bhead += read_size;

    return read_size;
}

size_t FileReadEx(File* file, u64 offset, void* buffer, size_t bsize, size_t bcount)
{
    u32 r_count;
    size_t total_rd_len = 0;
    uint8_t* i_buffer;

    if (!buffer || !bsize || !bcount || !file)
        return NULL;

    i_buffer = (uint8_t*)buffer;

    for (uint64_t i = 0; i < bcount; i++) {
        r_count = __read_bytes(file, offset, i_buffer, bsize);

        /* Increase the target buffer */
        i_buffer += r_count;

        /* Also increase the offset */
        offset += r_count;

        /* Count how many bytes we've read until now */
        total_rd_len += r_count;

        /* Check this read */
        if (r_count != bsize)
            break;
    }

    return total_rd_len;
}

static int __file_maybe_init_write_buffer(File* file, u64 offset)
{
    /* We already have a buffer, no need to do shit */
    if (file->wr_buff)
        return EOK;

    file->wr_buff = malloc(file->wr_bsize);

    if (!file->wr_buff)
        return -ENOMEM;

    /* Read from the backing object */
    ObjectRead(file->object, offset, file->wr_buff, file->wr_bsize);

    /* Set the buffer start offset */
    file->wr_file_offset = offset;
    file->wr_buffer_offset = 0;

    return 0;
}

/*!
 * @brief: Try to write in the files write buffer
 *
 * If the offset is inside the bounds of where the write buffer lays, we can write
 * @byte into it, so it can be pushed to the file at some point by FileFlush
 */
static inline int __file_try_write_in_buffer(File* file, u64 offset, u8 byte)
{
    /* Check if this write is inbetween the bounds of where we have our buffer currently */
    if (offset >= file->wr_file_offset && offset < (file->wr_file_offset + file->wr_bsize)) {
        /* Set the current buffer offset */
        file->wr_buffer_offset = offset - file->wr_file_offset;
        /* Write into the files write buffer */
        file->wr_buff[file->wr_buffer_offset] = byte;
        /* Increase the cache hit count */
        file->wr_cache_hit_count++;

        return 0;
    }

    return -1;
}

/*!
 * @brief: Write a single byte into the buffer of a stream
 *
 * Does some weird cache juggling to make sure this shit is kinda fast
 */
static int __write_byte(File* file, u64 offset, u8 byte)
{
    int error;

    /* If the buffer size isn't set for the file, we can't read like this */
    if (!file->wr_bsize)
        return -EINVAL;

    /* Buffer isn't yet set, create the first */
    error = __file_maybe_init_write_buffer(file, offset);

    if (error)
        return error;

    /* Check if we can simply write */
    if (IS_OK(__file_try_write_in_buffer(file, offset, byte)))
        return 0;

    /* Write is outside the write buffer, we need to sync */
    FileFlush(file);

    /* Try to read shit into the write buffer */
    ObjectRead(file->object, offset, file->wr_buff, file->wr_bsize);

    /* Set the new buffer start */
    file->wr_file_offset = offset;
    file->wr_buffer_offset = 0;
    /* This write did in fact hit the cache (we just synced lol) */
    file->wr_cache_hit_count = 1;

    /* Set the first byte in the buffer, since that's the one we want to access anyway */
    file->wr_buff[0] = byte;

    return 0;
}

/*
 * Write a string of bytes into the buffer of a stream
 *
 * Probably unsafe, since it just continues until it finds a null-byte
 */
static size_t __write_bytes(File* file, u64 offset, u8* bytes, size_t bsize)
{
    size_t write_len = 0;

    for (u64 i = 0; i < bsize; i++) {
        if (__write_byte(file, offset + i, bytes[i]))
            break;

        write_len++;
    }

    return write_len;
}

size_t FileWrite(File* file, void* buffer, size_t bsize)
{
    size_t write_size;

    write_size = FileWriteEx(file, file->wr_file_head, buffer, bsize, 1);

    file->wr_file_head += write_size;

    return write_size;
}

size_t FileWriteEx(File* file, u64 offset, void* buffer, size_t bsize, size_t bcount)
{
    u32 r_count;
    size_t total_rd_len = 0;
    uint8_t* i_buffer;

    if (!buffer || !bsize || !bcount || !file)
        return NULL;

    i_buffer = (uint8_t*)buffer;

    for (uint64_t i = 0; i < bcount; i++) {
        r_count = __write_bytes(file, offset, i_buffer, bsize);

        i_buffer += r_count;

        offset += r_count;

        /* Count how many bytes we've read until now */
        total_rd_len += r_count;

        /* Check if this read succeeded only partially */
        if (r_count < bsize)
            break;
    }

    return total_rd_len;
}

error_t FileFlush(File* file)
{
    if (!file || !file->wr_buff)
        return -1;

    /*
     * The syscall should handle empty buffers or invalid handles
     * but we'll check here just to be safe
     */
    if (!file->wr_cache_hit_count)
        return 0;

    /* This guy has no object, we can't flush lol */
    if (!ObjectIsValid(file->object))
        return 0;

    /* Write back the write buffer */
    ObjectWrite(file->object, file->wr_file_offset, file->wr_buff, file->wr_buffer_offset);

    memset(file->wr_buff, 0, file->wr_buffer_offset);

    /* Reset the cache hit count */
    file->wr_cache_hit_count = 0;
    file->wr_buffer_offset = NULL;

    return 0;
}

error_t FileSeekRHead(File* file, u64* offset, int whence)
{
    if (!file || !offset)
        return -EINVAL;

    switch (whence) {
    case SEEK_CUR:
        break;
    case SEEK_SET:
        file->rd_bhead = *offset;
        break;
    }

    *offset = file->rd_bhead;

    return 0;
}

error_t FileSeekWHead(File* file, u64* offset, int whence)
{
    if (!file || !offset)
        return -EINVAL;

    switch (whence) {
    case SEEK_CUR:
        break;
    case SEEK_SET:
        file->wr_file_head = *offset;
        break;
    }

    *offset = file->wr_file_head;

    return 0;
}

error_t FileSeek(File* file, u64* offset, int whence)
{
    if (FileSeekRHead(file, offset, whence))
        return -EINVAL;

    if (FileSeekWHead(file, offset, whence))
        return -EINVAL;

    return 0;
}

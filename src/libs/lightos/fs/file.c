#include "file.h"
#include "lightos/api/filesystem.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

File __create_file(u32 flags, Object object)
{
    File ret = { 0 };

    ret.object = object;

    if (!ObjectIsValid(&ret.object))
        return ret;

    /* Initialize file buffers */
    FileSetBuffers(&ret, LIGHTOS_FILE_DEFAULT_RBSIZE, (flags & OF_READONLY) ? 0 : LIGHTOS_FILE_DEFAULT_WBSIZE);

    return ret;
}

File CreateFile(const char* key, u32 flags)
{
    return __create_file(flags, CreateObject(key, flags, OT_FILE));
}

File OpenFile(const char* path, u32 hndl_flags, enum HNDL_MODE mode)
{
    return OpenFileFrom(nullptr, path, hndl_flags, mode);
}

File OpenFileFrom(Object* rel, const char* path, u32 hndl_flags, enum HNDL_MODE mode)
{
    Object object;

    if (!path)
        return (File) { 0 };

    object = OpenObjectFrom(rel, path, hndl_flags, mode);

    if (!ObjectIsValid(&object))
        return (File) { 0 };

    if (object.type != OT_FILE)
        CloseObject(rel);

    return __create_file((hndl_flags & HNDL_FLAG_W) ? NULL : OF_READONLY, object);
}

error_t CloseFile(File* file)
{
    if (file->rd_buff)
        free(file->rd_buff);

    if (file->wr_buff)
        free(file->wr_buff);

    return CloseObject(&file->object);
}

error_t FileSetBuffers(File* file, size_t r_bsize, size_t w_bsize)
{
    file->rd_bsize = r_bsize;
    file->wr_bsize = w_bsize;

    /* Free old buffers if we have those */
    if (file->rd_buff)
        free(file->rd_buff);

    if (file->wr_buff)
        free(file->wr_buff);

    /* Now allocate new buffers */
    if (r_bsize) {
        file->rd_buff = malloc(file->rd_bsize);
        memset(file->rd_buff, 0, r_bsize);
    }

    if (w_bsize) {
        file->wr_buff = malloc(file->wr_bsize);
        memset(file->wr_buff, 0, w_bsize);
    }

    return 0;
}

bool FileIsValid(File* file)
{
    return ObjectIsValid(&file->object);
}

int __read_byte(File* file, uint8_t* buffer)
{
    size_t r_size;

    if (!buffer || !file->rd_buff)
        return NULL;

    /* We have run out of local buffer space. Lets ask for some new */
    if (!file->rd_capacity) {

        memset(file->rd_buff, 0, file->rd_bsize);

        if (ObjectRead(&file->object, file->head, file->rd_buff, file->rd_bsize))
            return NULL;

        r_size = file->rd_bsize;

        /* Reset the read offset */
        file->rd_boffset = NULL;

        /* Set capacity for the next read */
        file->rd_capacity = r_size;
    }

    *buffer = file->rd_buff[file->rd_boffset];

    file->rd_boffset++;
    file->rd_capacity--;

    return 1;
}

int __read_bytes(File* file, uint8_t* buffer, size_t size)
{
    int ret = 0;
    int result;

    for (uint64_t i = 0; i < size; i++) {
        result = __read_byte(file, &buffer[i]);

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

    read_size = FileReadEx(file, file->head, buffer, bsize, 1);

    /* Increase the read offset */
    file->head += read_size;

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
        r_count = __read_bytes(file, i_buffer, bsize);

        i_buffer += r_count;

        /* Count how many bytes we've read until now */
        total_rd_len += r_count;

        /* Check this read */
        if (r_count != bsize)
            break;
    }

    return total_rd_len;
}

/*
 * Write a single byte into the buffer of a stream
 */
static int __write_byte(File* file, uint64_t* counter, char byte)
{
    if (!file->wr_buff)
        return -1;

    file->wr_buff[file->wr_boffset++] = byte;

    /* Sync the buffer if the max. buffersize is reached, or the byte is a null-char */
    if (file->wr_boffset >= file->wr_bsize)
        FileFlush(file);

    if (counter)
        (*counter)++;

    return 0;
}

/*
 * Write a string of bytes into the buffer of a stream
 *
 * Probably unsafe, since it just continues until it finds a null-byte
 */
static size_t __write_bytes(File* file, uint64_t* counter, u8* bytes, size_t bsize)
{
    size_t write_len = 0;

    for (u64 i = 0; i < bsize; i++) {
        if (__write_byte(file, counter, bytes[i]))
            break;

        write_len++;
    }

    return write_len;
}

size_t FileWrite(File* file, void* buffer, size_t bsize)
{
    size_t write_size;

    write_size = FileWriteEx(file, file->head, buffer, bsize, 1);

    /* Increase the read offset */
    file->head += write_size;

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
        r_count = __write_bytes(file, NULL, i_buffer, bsize);

        i_buffer += r_count;

        /* Count how many bytes we've read until now */
        total_rd_len += r_count;

        /* Check this read */
        if (r_count != bsize)
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
    if (!file->wr_boffset)
        return 0;

    /* This guy has no object, we can't flush lol */
    if (!ObjectIsValid(&file->object))
        return 0;

    ObjectWrite(&file->object, file->head, file->wr_buff, file->wr_boffset);

    /* Reset the buffer */
    memset(file->wr_buff, 0, file->wr_boffset);

    /* Reset the written count */
    file->wr_boffset = NULL;

    return 0;
}

error_t FileSeek(File* file, u64* offset, int whence)
{
    if (!file || !offset)
        return -EINVAL;

    /* Flush the write buffer */
    FileFlush(file);

    switch (whence) {
    case SEEK_CUR:
        break;
    case SEEK_SET:
        file->head = *offset;
        break;
    }

    *offset = file->head;

    return 0;
}

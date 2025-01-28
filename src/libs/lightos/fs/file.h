#ifndef __LIGHTENV_FS_FILE__
#define __LIGHTENV_FS_FILE__

#include <lightos/api/filesystem.h>
#include <lightos/api/handle.h>
#include <lightos/types.h>

File* CreateFile(const char* key, u32 flags);
File* OpenFile(const char* path, u32 hndl_flags, enum HNDL_MODE mode);
File* OpenFileFrom(Object* obj, const char* path, u32 hndl_flags, enum HNDL_MODE mode);

error_t CloseFile(File* file);
error_t FileSetBuffers(File* file, size_t r_bsize, size_t w_bsize);

bool FileIsValid(File* file);

size_t FileRead(File* file, void* buffer, size_t bsize);
size_t FileReadEx(File* file, u64 offset, void* buffer, size_t bsize, size_t bcount);

size_t FileWrite(File* file, void* buffer, size_t bsize);
size_t FileWriteEx(File* file, u64 offset, void* buffer, size_t bsize, size_t bcount);

error_t FileSeek(File* file, u64* offset, int whence);
error_t FileSeekRHead(File* file, u64* offset, int whence);
error_t FileSeekWHead(File* file, u64* offset, int whence);
error_t FileFlush(File* file);

#endif // !__LIGHTENV_FS_FILE__

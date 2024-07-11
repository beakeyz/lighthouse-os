#include "dir.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "lightos/system.h"
#include <stdlib.h>
#include <string.h>

Directory* open_dir(const char* path, DWORD flags, DWORD mode)
{
    HANDLE handle;
    Directory* dir;

    handle = open_handle(path, HNDL_TYPE_DIR, flags, mode);

    if (!handle_verify(handle))
        return nullptr;

    dir = malloc(sizeof(*dir));

    if (!dir)
        goto close_and_exit;

    memset(dir, 0, sizeof(*dir));

    dir->handle = handle;
    dir->entries = nullptr;

    return dir;

close_and_exit:
    close_handle(handle);
    return nullptr;
}

void close_dir(Directory* dir)
{
    DirEntry* c_entry;

    while (dir->entries) {
        c_entry = dir->entries;
        dir->entries = dir->entries->next;

        free(c_entry);
    }

    close_handle(dir->handle);
    free(dir);
}

static DirEntry* _dir_read_entry(Directory* dir, uint32_t idx)
{
    DirEntry* walker;

    if (!dir->entries)
        return nullptr;

    walker = dir->entries;

    do {
        if (walker->idx == idx)
            return walker;

        if (walker->idx > idx)
            break;

        walker = walker->next;
    } while (walker);

    return nullptr;
}

/*!
 * @brief: Insert a directory entry into the directory cache
 *
 * We sort the entries based on index, so we can easily find them
 * FIXME: The indices should change when we try to remove or add entries?
 */
static void _dir_link_direntry(Directory* dir, DirEntry* entry)
{
    DirEntry** walker;

    if (!dir->entries) {
        dir->entries = entry;
        return;
    }

    walker = &dir->entries;

    do {

        /* If this entry is a higher index than the one we're currently at, we just
           smash it into the list =0 */
        if (entry->idx > (*walker)->idx) {
            entry->next = (*walker)->next;
            (*walker)->next = entry;
            return;
        }

        walker = &(*walker)->next;
    } while (*walker);

    if (!(*walker))
        *walker = entry;
}

DirEntry* dir_read_entry(Directory* dir, uint32_t idx)
{
    DirEntry* ent;
    syscall_result_t res;

    /* Check if we have the entry cached */
    ent = _dir_read_entry(dir, idx);

    if (ent)
        return ent;

    /* Nope, make a new one */
    ent = malloc(sizeof(*ent));

    if (!ent)
        return nullptr;

    memset(ent, 0, sizeof(*ent));

    ent->idx = idx;

    /* Ask kernel uwu */
    res = syscall_4(SYSID_DIR_READ, dir->handle, idx, (uint64_t)&ent->entry.name, sizeof(ent->entry.name));

    if (res != SYS_OK)
        goto dealloc_and_exit;

    /* Make sure the last byte is null */
    ent->entry.name[sizeof(ent->entry.name) - 1] = '\0';

    _dir_link_direntry(dir, ent);

    return ent;
dealloc_and_exit:
    free(ent);
    return nullptr;
}

HANDLE dir_entry_open(
    __IN__ DirEntry* entry);

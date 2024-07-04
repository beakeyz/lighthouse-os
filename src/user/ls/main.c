#include "lightos/fs/dir.h"
#include "lightos/proc/cmdline.h"
#include "sys/types.h"
#include <kterm/kterm.h>
#include <stdio.h>
#include <stdlib.h>

CMDLINE line;

struct list_entry {
    DirEntry* entry;
    struct list_entry* next;
};

static int list_add(struct list_entry** list_head, DirEntry* entry)
{
    if (!list_head)
        return -1;

    while (*list_head)
        list_head = &(*list_head)->next;

    *list_head = malloc(sizeof(struct list_entry));
    (*list_head)->entry = entry;
    (*list_head)->next = nullptr;

    return 0;
}

static int list_switch_next(struct list_entry** a)
{
    struct list_entry* cached_next;
    struct list_entry* cached_next_next;

    if (!a)
        return -1;

    cached_next = (*a)->next;

    if ((*a)->next) {
        cached_next_next = (*a)->next->next;

        (*a)->next->next = *a;
        (*a)->next = cached_next_next;
    }

    *a = cached_next;

    return 0;
}

static int destroy_list(struct list_entry* list_head)
{
    struct list_entry* next;

    if (!list_head)
        return -1;

    do {
        next = list_head->next;

        free(list_head);

        list_head = next;
    } while (list_head);

    return 0;
}

static const char* _ls_get_target_path()
{
    const char* ret;

    for (uint32_t i = 1; i < line.argc; i++) {
        if (!line.argv[i])
            continue;

        if (line.argv[i][0] == '-')
            continue;

        return line.argv[i];
    }

    /* Grab the cwd from kterm if we need to */
    if (kterm_get_cwd(&ret))
        return nullptr;

    return ret;
}

static void _fill_semi_sorted_dirlist(struct list_entry** sorted, struct list_entry** unsorted)
{
    struct list_entry** c_entry;

    c_entry = unsorted;

    /* First add the directories */
    while (*c_entry) {
        if (direntry_is_dir((*c_entry)->entry))
            list_add(sorted, (*c_entry)->entry);
        c_entry = &(*c_entry)->next;
    }

    c_entry = unsorted;

    /* Then add the normal entries */
    while (*c_entry) {
        if (!direntry_is_dir((*c_entry)->entry))
            list_add(sorted, (*c_entry)->entry);
        c_entry = &(*c_entry)->next;
    }
}

static void _ls_print_direntry(DirEntry* entry)
{
    printf(" %s%s\n", entry->entry.name, direntry_is_dir(entry) ? "/" : " ");
}

/*!
 * The ls binary for lightos
 *
 * pretty much just copied the manpage of the linux version of this utility.
 * There are a few different things though. Since lightos does not give an argv
 * though the program execution pipeline, we need to ask lightos for our commandline.
 */
int main()
{
    int error;
    uint64_t idx;
    const char* path;
    Directory* dir;
    DirEntry* entry;
    struct list_entry* dirlist;
    struct list_entry* dirlist_semi_sorted;

    error = cmdline_get(&line);

    if (error)
        return error;

    path = _ls_get_target_path();

    idx = 0;
    dir = open_dir(path, NULL, NULL);

    if (!dir) {
        printf("Invalid path (Not a directory?)\n");
        return 0;
    }

    do {
        entry = dir_read_entry(dir, idx);

        if (!entry)
            break;

        list_add(&dirlist, entry);
        idx++;
    } while (true);

    /* No entries */
    if (!dirlist)
        goto close_and_exit;

    dirlist_semi_sorted = nullptr;

    /* Semi sort */
    _fill_semi_sorted_dirlist(&dirlist_semi_sorted, &dirlist);

    /* Kill the first list */
    destroy_list(dirlist);

    dirlist = dirlist_semi_sorted;

    do {
        _ls_print_direntry(dirlist->entry);

        dirlist = dirlist->next;
    } while (dirlist);

    /* Murder */
    destroy_list(dirlist_semi_sorted);

close_and_exit:
    close_dir(dir);
    return 0;
}

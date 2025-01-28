#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/proc/cmdline.h"
#include "lsdk/lists/list.h"
#include <lightos/handle.h>
#include <lightos/object.h>
#include <stdio.h>
#include <string.h>

/* Holds the relative path where we want to search from */
const char* __relative_path = NULL;
static list_t* __entry_list;

static void __parse_flag(const char* flag_arg)
{
}

static error_t __parse_cmdline(u32 argc)
{
    const char* arg;

    if (!argc)
        return 0;

    for (u32 i = 1; i < argc; i++) {
        arg = cmdline_get_arg(i);

        if (!arg)
            continue;

        if (arg[0] == '-') {
            __parse_flag(arg);
            continue;
        }

        if (!__relative_path)
            __relative_path = arg;
    }

    return 0;
}

static Object* __get_target_object(Object* self_obj)
{
    if (__relative_path)
        return OpenObject(__relative_path, HF_R, NULL);

    /* Try to open the working object holder */
    return OpenWorkingObject(HF_R);
}

static error_t __maybe_add_object(Object* obj)
{
    foreach_list(i, __entry_list)
    {
        Object* cur_obj = i->data;

        /* Duplicate entry? */
        if (strncmp(cur_obj->key, obj->key, sizeof(obj->key)) == 0)
            return -EDUPLICATE;
    }

    list_add(__entry_list, obj);
    return 0;
}

error_t main(HANDLE self)
{
    error_t error;
    /* Grab the commandline argument count */
    u32 argc = cmdline_get_argc();
    u32 idx = 0;

    Object* rel_obj;
    Object self_obj = {
        .handle = self,
        .type = OT_PROCESS,
    };

    /* Try to parse the command line (Shouldn't fail lol) */
    error = __parse_cmdline(argc);

    if (error)
        return error;

    rel_obj = __get_target_object(&self_obj);

    if (!ObjectIsValid(rel_obj))
        return -ENOENT;

    /* Create a list for the entries */
    __entry_list = create_list();

    /* First, gather all the non-connected objects */
    for (idx = 0;; idx++) {
        /* Grab this guy */
        Object* walker = OpenObjectIdx(rel_obj, idx, HF_R, NULL);

        if (!ObjectIsValid(walker))
            break;

        /* Add thie entry to the list */
        list_add(__entry_list, walker);
    }

    if (__entry_list->head != nullptr)
        goto itterate;

    /* Then, gather the connected, non-duplicate objects */
    for (idx = 0;; idx++) {
        /* Grab this guy */
        Object* walker = OpenConnectedObjectByIdx(rel_obj, idx, HF_R, NULL);

        if (!ObjectIsValid(walker))
            break;

        error = __maybe_add_object(walker);

        if (error)
            /* Also make sure to close the object */
            CloseObject(walker);
    }

itterate:
    foreach_list(i, __entry_list)
    {
        Object* walker = i->data;

        printf("%s%s\n", walker->key, (walker->type == OT_DIR || walker->type == OT_DGROUP || walker->type == OT_GENERIC) ? "/" : "");

        /* Close the object */
        CloseObject(walker);
    }

    destroy_list(__entry_list);

    CloseObject(rel_obj);

    return 0;
}

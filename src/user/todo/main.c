#include "lightos/api/handle.h"
#include <lightos/fs/file.h>
#include <lightos/lightos.h>
#include <lightos/proc/cmdline.h>
#include <stdio.h>

static const char* __output = NULL;
static const char* __new_name = NULL;

static inline int print_usage()
{
    puts(
        "Usage: todo [flags]... [item] \n"
        "Manage tasks and deadlines\n"
        "\n"
        "Flags:\n"
        "  -o [file]            Specify an output file\n"
        "  -n [name]            Add a new todo, with the name [name]\n"
        "\n");
    return -EBADPARAMS;
}

const char* consume_arg(u32* p_idx)
{
    const char* ret;

    ret = cmdline_get_arg(*p_idx);

    (*p_idx)++;

    return ret;
}

static int __parse_args()
{
    u32 argc = cmdline_get_argc();

    if (argc == 1)
        return print_usage();

    u32 i = 1;

    do {
        const char* arg = consume_arg(&i);

        if (!arg)
            break;

        /* Skip non flag entries */
        if (arg[0] != '-')
            continue;

        switch (arg[1]) {
        case 'o':
            __output = consume_arg(&i);
            break;
        case 'n':
            __new_name = consume_arg(&i);
            break;
        }
    } while (true);

    return 0;
}

int main(HANDLE self)
{
    File* output_file;
    int error;

    error = __parse_args();

    if (error)
        return error;

    /* Maybe create a new file. It might already exist, in which case we get an existing handle */
    output_file = OpenFile(__output, HF_RW, HNDL_MODE_CREATE);

    if (!output_file)
        return -EBADF;

    CloseFile(output_file);

    return 0;
}

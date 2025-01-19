#include "defaults.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Buffers that get placed into the file */
pvr_file_header_t pvr_hdr;
static pvr_file_var_buffer_t* var_buffer;
static pvr_file_strtab_t* strtab_buffer;

/* Sizes for this buffer */
static uint32_t var_buffersize;
static uint32_t strtab_buffersize;

/*
 * NOTE: we can't store a default password here, since they have to be hashed by the kernel
 * when they are stored
 */
struct __sysvar_template base_defaults[] = {};

/*
 * Default values for the global profile
 * These include mostly just paths to drivers, kobjects, ect.
 */
struct __sysvar_template user_defaults[] = {
    VAR_ENTRY("DFLT_LWND_PATH", SYSVAR_TYPE_STRING, "service/lwnd", SYSVAR_FLAG_CONFIG),
    VAR_ENTRY("DFLT_ERR_EVENT", SYSVAR_TYPE_STRING, "error", SYSVAR_FLAG_CONFIG),
    VAR_ENTRY("BOOTDISK_PATH", SYSVAR_TYPE_STRING, "unknown", SYSVAR_FLAG_CONFIG),
    VAR_ENTRY("LOGIN_MSG", SYSVAR_TYPE_STRING, "Welcome to LightOS!", SYSVAR_FLAG_GLOBAL),
    VAR_ENTRY("EPIC_TEST", SYSVAR_TYPE_WORD, 69, 0),
};

struct __sysvar_template admin_defaults[] = {
    VAR_ENTRY("LOGIN_MSG", SYSVAR_TYPE_STRING, "Welcome to LightOS, dearest Admin! (Try not to break shit)", SYSVAR_FLAG_GLOBAL),
    VAR_ENTRY("PATH", SYSVAR_TYPE_STRING, "Storage/Root/Users/Admin/Core:Storage/Root/Apps", SYSVAR_FLAG_GLOBAL),
};

struct __sysvar_template dispmgr_defaults[] = {
    VAR_ENTRY("KEYDEV", SYSVAR_TYPE_STRING, "Dev/hid/i8042", SYSVAR_FLAG_CONFIG),
    VAR_ENTRY("MOUSEDEV", SYSVAR_TYPE_STRING, "None", SYSVAR_FLAG_CONFIG),
    VAR_ENTRY("VIDDEV", SYSVAR_TYPE_STRING, "Dev/video/maindev", SYSVAR_FLAG_CONFIG),
    VAR_ENTRY("AUDIODEV", SYSVAR_TYPE_STRING, "None", SYSVAR_FLAG_CONFIG),
    /* Video modes are going to be indexed and mode zero is going to be the default mode (1920x1080@60Hz or something) */
    VAR_ENTRY("VIDMODE", SYSVAR_TYPE_DWORD, 0, SYSVAR_FLAG_CONFIG),
    /* 0% -> 100% */
    VAR_ENTRY("AUDIOVOLUME", SYSVAR_TYPE_DWORD, 100, SYSVAR_FLAG_CONFIG),
};

static inline pvr_file_strtab_entry_t* pvr_file_get_strtab_entry(uint32_t offset)
{
    return (pvr_file_strtab_entry_t*)((void*)strtab_buffer->entries + offset);
}

static int pvr_file_find_free_strtab_offset(uint32_t* offset)
{
    uint32_t i;
    pvr_file_strtab_entry_t* c_entry;

    i = 0;

    do {
        c_entry = pvr_file_get_strtab_entry(i);

        if (!c_entry->len)
            break;

        /* Go to next entry */
        i += sizeof(*c_entry) + c_entry->len;
    } while (i < strtab_buffer->bytecount);

    if (i >= strtab_buffer->bytecount)
        return -1;

    *offset = i;
    return 0;
}

static int pvr_file_find_free_valtab_offset(uint32_t* offset)
{
    uint32_t i;

    for (i = 0; i < var_buffer->var_capacity; i++) {
        if (!PVR_VAR_ISUSED(&var_buffer->variables[i]))
            break;
    }

    if (i >= var_buffer->var_capacity)
        return -1;

    *offset = i;
    return 0;
}

static int pvr_file_add_variable(struct __sysvar_template* var)
{
    uint8_t buffer_bounds_respected;
    uint32_t valtab_offset;
    uint32_t strtab_offset;
    size_t buffersize;
    pvr_file_var_t* c_var;
    pvr_file_strtab_entry_t* c_str;

    c_var = &var_buffer->variables[0];
    buffersize = 0;
    valtab_offset = strtab_offset = 0;

    /* Find an unused var */
    while ((buffer_bounds_respected = (buffersize + sizeof(*c_var)) < var_buffersize)) {

        if (c_var->var_type == SYSVAR_TYPE_UNSET)
            break;

        c_var++;
        buffersize += sizeof(*c_var);
    }

    /* Could not find a free var */
    if (!buffer_bounds_respected)
        return -1;

    pvr_file_find_free_valtab_offset(&valtab_offset);

    if (var->type == SYSVAR_TYPE_STRING) {
        pvr_file_find_free_strtab_offset(&strtab_offset);

        c_str = pvr_file_get_strtab_entry(strtab_offset);
        c_str->len = strlen(var->value);

        /* Let's trust that there is a string in ->value =) */
        strncpy(
            c_str->str,
            var->value,
            c_str->len);

        printf("%s at offset (%d)\n", (const char*)var->value, strtab_offset);

        /* Set the offset */
        c_var->var_value = strtab_offset;
    } else {
        c_var->var_value = (uintptr_t)var->value;
    }

    pvr_file_find_free_strtab_offset(&strtab_offset);

    c_str = pvr_file_get_strtab_entry(strtab_offset);
    c_str->len = strlen(var->key);

    /* Let's trust that there is a string in ->value =) */
    strncpy(
        c_str->str,
        var->key,
        c_str->len);

    /* Set the c_var */
    c_var->key_off = strtab_offset;
    c_var->var_type = var->type;
    c_var->var_flags = var->flags;

    var_buffer->var_count++;
    return 0;
}

static void init_fvar_buffer(uint32_t buffersize)
{
    pvr_file_var_t* c_var;
    size_t current_size;

    var_buffersize = buffersize;
    var_buffer = malloc(buffersize);

    /* Clear the buffer */
    memset((void*)var_buffer, 0, buffersize);

    /* Set our capacity */
    var_buffer->var_capacity = (buffersize) / sizeof(pvr_file_var_t);
}

static void init_strtab_buffer(uint32_t buffersize)
{
    strtab_buffersize = buffersize;
    strtab_buffer = malloc(buffersize);

    memset((void*)strtab_buffer, 0, buffersize);

    strtab_buffer->bytecount = buffersize - sizeof(*strtab_buffer);
}

static const char* _get_output_file(int argc, char** argv)
{
    for (uint32_t i = 0; i < argc; i++) {
        if (strncmp(argv[i], "-o", 2) != 0)
            continue;

        return argv[i + 1];
    }

    return NULL;
}

static int base_env_save_to_file(const char* path, struct __sysvar_template* templates, uint32_t count)
{
    size_t cur_offset;
    memset(&pvr_hdr, 0, sizeof(pvr_hdr));
    memcpy((void*)&pvr_hdr.sign, SYSVAR_SIG, 4);

    init_fvar_buffer(sizeof(pvr_file_var_t) * DEFAULT_VAR_CAPACITY);
    init_strtab_buffer(sizeof(pvr_file_strtab_t) + DEFAULT_STRTAB_BYTECOUNT);

    FILE* f = fopen(path, "w");

    if (!f)
        return -1;

    for (uint32_t i = 0; i < count; i++)
        pvr_file_add_variable(&templates[i]);

    /*
     * Write the entire thing to disk
     */
    {
        /* Start by writing the variables */
        fseek(f, sizeof(pvr_hdr), SEEK_SET);
        cur_offset = sizeof(pvr_hdr);

        /* Set the offset of the first variable buffer */
        pvr_hdr.varbuf_offset = cur_offset;

        /* Write the entire buffer to the file */
        cur_offset += var_buffersize;
        fwrite(var_buffer, var_buffersize, 1, f);

        /* Set the offset of the first string table */
        pvr_hdr.strtab_offset = cur_offset;

        /* Write the entire thing to file */
        cur_offset += strtab_buffersize;
        fwrite(strtab_buffer, strtab_buffersize, 1, f);

        /* Write the header last */
        fseek(f, 0, SEEK_SET);
        fwrite(&pvr_hdr, sizeof(pvr_hdr), 1, f);

        fclose(f);
    }

    free(var_buffer);
    free(strtab_buffer);
    return 0;
}

static bool admin_template = false;

static void _parse_args(int argc, char** argv)
{
    char* c_arg;

    for (int i = 0; i < argc; i++) {
        c_arg = argv[i];

        if (*c_arg != '-')
            continue;

        if (strncmp(c_arg, "--admin-template", 16) == 0)
            admin_template = true;
    }
}

/*!
 * @brief: Tool to create .pvr files for lightos
 *
 * .pvr (dot profile variable) files are files that contain profile variables for a specific profile
 * They are used by the aniva kernel to read in saved profile variables for any profiles during system boot.
 * The kernel is able to create its own variable files, but we need some sort of bootstrap, which this tool
 * is for.
 *
 * NOTE: the name of the .pvr file should be the same as the profile name it targets. This means there can't be two
 * files in the same directory, that target the same profile.
 *
 * Usage:
 * This tool can be used to create, modify, test, and install .pvr files. This is how it's used:
 *  base_env [target path] [flags] <flag args>
 * The target path is relative to the /system directory in the project root, which serves as an image for the ramdisk
 * and the systems initial fs state.
 * Accepted flags are:
 *  -B : Create the default .pvr file for the BASE profile
 *  -G : Create the default .pvr file for the Global profile
 *  -e <VAR NAME> <new value> : Edits a variable with a value of the same type
 *  -r <VAR NAME> : Removes a variable
 *  -t <VAR NAME> <NEW TYPE> : Change the type of a variable
 *
 * File layout:
 * The layout of a .pvr file is closely related to how an ELF file is laid out. There is a header which defines some
 * initial offsets for the value and string tables. The var table starts directly after the header, so there is no offset needed there
 *
 * TODO: parse arguments
 */
int main(int argc, char** argv)
{
    const char* output_file;
    uint32_t count;
    printf("Welcome to the base_env manager\n");

    /* Parse input args */
    _parse_args(argc, argv);

    output_file = _get_output_file(argc, argv);

    if (!output_file)
        output_file = "out.pvr";

    count = (sizeof user_defaults / sizeof user_defaults[0]);

    if (admin_template)
        count = (sizeof admin_defaults / sizeof admin_defaults[0]);

    return base_env_save_to_file(output_file, admin_template ? admin_defaults : user_defaults, count);
}

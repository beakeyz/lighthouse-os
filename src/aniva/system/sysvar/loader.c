#include "loader.h"
#include "entry/entry.h"
#include "fs/file.h"
#include "lightos/sysvar/shared.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "oss/node.h"
#include "sync/mutex.h"
#include "system/sysvar/map.h"
#include "system/sysvar/var.h"
#include <libk/string.h>

static inline bool is_header_valid(pvr_file_header_t* hdr)
{
    return (
        hdr->sign[0] == SYSVAR_SIG_0 && hdr->sign[1] == SYSVAR_SIG_1 && hdr->sign[2] == SYSVAR_SIG_2 && hdr->sign[3] == SYSVAR_SIG_3);
}

static inline uintptr_t get_abs_strtab_entry_offset(pvr_file_header_t* hdr, uint32_t strtab_entry_off)
{
    return (hdr->strtab_offset + sizeof(pvr_file_strtab_t) + strtab_entry_off);
}

static inline int get_strlen(struct file* file, pvr_file_header_t* hdr, uint32_t offset, uint16_t* p_len)
{
    size_t read_size;
    uint16_t len;

    if (!p_len)
        return -1;

    /* Read from the file */
    read_size = file_read(file, &len, sizeof(len), offset);

    if (read_size != sizeof(len))
        return -1;

    *p_len = len;
    return 0;
}

static inline const char* get_str(struct file* file, pvr_file_header_t* hdr, uint32_t strtab_entry_off)
{
    uintptr_t abs_offset;
    char* key_buffer;
    uint16_t c_strlen = 0;

    abs_offset = get_abs_strtab_entry_offset(hdr, strtab_entry_off);

    /* Get the length of the key */
    if (get_strlen(file, hdr, abs_offset, &c_strlen))
        return nullptr;

    /* Allocate the buffer */
    key_buffer = kmalloc(c_strlen + 1);

    /* Set the bastard to zero */
    memset(key_buffer, 0, c_strlen + 1);

    /* Get the key string */
    if (file_read(file, key_buffer, c_strlen, abs_offset + sizeof(uint16_t)) != c_strlen) {
        kfree(key_buffer);
        return nullptr;
    }

    return key_buffer;
}

int sysvarldr_load_variables(struct oss_node* node, uint16_t priv_lvl, struct file* file)
{
    uintptr_t c_var_offset;
    uintptr_t c_var_count;
    sysvar_t* loaded_var;
    pvr_file_var_buffer_t c_varbuf;
    pvr_file_var_t c_var = { 0 };
    pvr_file_header_t hdr = { 0 };

    const char* key_buffer;
    void* val_buffer;
    uintptr_t c_value;

    file_read(file, &hdr, sizeof(hdr), 0);

    if (!is_header_valid(&hdr))
        return -1;

    /* Init the variable count */
    c_var_count = 0;

    /* Var buffer starts directly after the header */
    c_var_offset = hdr.varbuf_offset + sizeof(c_varbuf);

    /* Fully lock the map during this opperation */
    mutex_lock(node->lock);

    file_read(file, &c_varbuf, sizeof(c_varbuf), hdr.varbuf_offset);

    printf("Trying to parse %d/%d vars from %s!\n", c_varbuf.var_count, c_varbuf.var_capacity, file->m_obj->name);

    for (uint32_t i = 0; c_var_count < c_varbuf.var_count && i < c_varbuf.var_capacity; i++) {
        file_read(file, &c_var, sizeof(c_var), c_var_offset);

        /* Skip unset variables */
        if (c_var.var_type == SYSVAR_TYPE_UNSET)
            continue;

        loaded_var = nullptr;

        /* Get the variable key */
        key_buffer = get_str(file, &hdr, c_var.key_off);

        /* Copy the correct value */
        if (c_var.var_type == SYSVAR_TYPE_STRING)
            val_buffer = (void*)get_str(file, &hdr, c_var.var_value);
        else {
            val_buffer = kmalloc(sizeof(uintptr_t));

            memcpy((void*)val_buffer, &c_var.var_value, sizeof(uintptr_t));
        }

        /* Try to grab this variable */
        loaded_var = sysvar_get(node, key_buffer);

        /* Ik this is very confusing, future me, but trust me on this one (Gr. past you) */
        c_value = ((c_var.var_type == SYSVAR_TYPE_STRING) ?
                                                          /* sysvar_write expects a pointer to a string (which it can strdup) when a variable is a string */
                (uintptr_t)val_buffer
                                                          :
                                                          /* Otherwise it will just put in the raw integer value we pass into 'value' */
                *(uintptr_t*)val_buffer);

        /* Do we already have this variable? */
        if (loaded_var != nullptr) {
            /* Overrride */
            sysvar_write(
                loaded_var,
                c_value);

            /* Release the reference made by sysvar_map_get */
            release_sysvar(loaded_var);
            goto cycle;
        }

        /* Put the variable */
        (void)sysvar_attach(node,
            key_buffer,
            priv_lvl,
            c_var.var_type,
            c_var.var_flags,
            c_value);

    cycle:
        /* Found a variable */
        c_var_count++;
        /* Free temporary buffers */
        kfree(val_buffer);
        kfree((void*)key_buffer);
        c_var_offset += sizeof(c_var);
    }

    mutex_unlock(node->lock);
    return 0;
}

static inline void write_str(file_t* file, pvr_file_header_t* hdr, uint16_t len, const char* str, uintptr_t* c_strtab_offset)
{
    uint32_t total_len = 2 + len;
    uint8_t buffer[total_len];

    memcpy(&buffer[0], &len, 2);
    memcpy(&buffer[2], str, len);

    /* Write the length */
    file_write(
        file,
        buffer,
        total_len,
        hdr->strtab_offset + sizeof(pvr_file_strtab_t) + *c_strtab_offset);

    *c_strtab_offset += total_len;
}

int sysvarldr_save_variables(struct oss_node* node, uint16_t priv_lvl, struct file* file)
{
    int error;
    /* Buffer element sizes */
    size_t var_array_len;
    size_t var_buffer_len;
    size_t var_strtab_len;

    /* Index offsets */
    size_t c_strlen;
    uintptr_t c_strtab_offset = 0;

    sysvar_t** var_array;
    pvr_file_var_t c_var = { 0 };
    pvr_file_var_buffer_t varbuf = { 0 };
    pvr_file_strtab_t strtab = { 0 };
    pvr_file_header_t hdr = { 0 };

    /* Can't save to a file that's read only */
    if ((file->m_flags & FILE_READONLY) == FILE_READONLY)
        return -1;

    /* Dump the variables into an array */
    error = sysvar_dump(node, &var_array, &var_array_len);

    if (error)
        return error;

    /* Copy the signature */
    memcpy((void*)hdr.sign, SYSVAR_SIG, 4);
    /* This file won't be expandable */
    hdr.kernel_version = g_system_info.kernel_version;

    var_buffer_len = ALIGN_UP(var_array_len * sizeof(pvr_file_var_t), 512);
    /* Give a theoretical 256 bytes per variable */
    var_strtab_len = ALIGN_UP(sizeof(pvr_file_strtab_t) + var_array_len * 256, 512);

    /* Set header offsets */
    hdr.varbuf_offset = sizeof(hdr);
    hdr.strtab_offset = hdr.varbuf_offset + var_buffer_len;

    varbuf.var_capacity = var_array_len;
    varbuf.var_count = var_array_len;

    strtab.bytecount = var_strtab_len - sizeof(pvr_file_strtab_t);
    strtab.next_valtab_off = 0;

    /* Loop over all the variables */
    for (uint64_t i = 0; i < var_array_len; i++) {
        c_strlen = strlen(var_array[i]->key);

        /* Clear the variable field */
        memset(&c_var, 0, sizeof(c_var));

        /* Yikes, have to skip */
        if (c_strlen > 0xffff) {
            /* Write a clear variable into this field, just in case */
            file_write(
                file,
                &c_var,
                sizeof(c_var),
                hdr.varbuf_offset + sizeof(pvr_file_var_buffer_t) + (i * sizeof(pvr_file_var_t)));

            continue;
        }

        c_var.var_type = var_array[i]->type;
        c_var.var_flags = var_array[i]->flags;
        c_var.key_off = c_strtab_offset;

        /* Write the key of the variable into the stringtable */
        write_str(file, &hdr, c_strlen, var_array[i]->key, &c_strtab_offset);

        switch (var_array[i]->type) {
        case SYSVAR_TYPE_STRING:
            /* Grab the length of the value */
            c_strlen = strlen(var_array[i]->value);

            /* Set the internal value */
            c_var.var_value = c_strtab_offset;

            /* Write the string into the string table */
            write_str(file, &hdr, c_strlen, var_array[i]->value, &c_strtab_offset);
            break;
        default:
            c_var.var_value = (uintptr_t)var_array[i]->value;
            break;
        }

        /* Write the variable to the file */
        file_write(
            file,
            &c_var,
            sizeof(c_var),
            hdr.varbuf_offset + sizeof(pvr_file_var_buffer_t) + (i * sizeof(pvr_file_var_t)));
    }

    file_write(file, &varbuf, sizeof(varbuf), hdr.varbuf_offset);
    file_write(file, &strtab, sizeof(strtab), hdr.strtab_offset);

    /* Write the header to the file */
    file_write(file, &hdr, sizeof(hdr), 0);

    /* Quick cache dump =) */
    file_sync(file);

    /* Free the array */
    kfree(var_array);
    return error;
}

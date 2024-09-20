#include "var.h"
#include "libk/flow/error.h"
#include "lightos/sysvar/shared.h"
#include "mem/heap.h"
#include "mem/zalloc/zalloc.h"
#include "oss/obj.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include <libk/string.h>

static zone_allocator_t __var_allocator;

static uint32_t profile_var_get_size_for_type(sysvar_t* var)
{
    if (!var)
        return 0;

    switch (var->type) {
    case SYSVAR_TYPE_BYTE:
        return sizeof(uint8_t);
    case SYSVAR_TYPE_WORD:
        return sizeof(uint16_t);
    case SYSVAR_TYPE_DWORD:
        return sizeof(uint32_t);
    case SYSVAR_TYPE_QWORD:
        return sizeof(uint64_t);
    case SYSVAR_TYPE_STRING:
        if (!var->str_value)
            return 0;

        return strlen(var->str_value) + 1;
    default:
        break;
    }
    return 0;
}

/*!
 * @brief: Deallocates a variables memory and removes it from its profile if it still has one
 *
 * Should not be called directly, but only by release_profile_var once its refcount has dropped to zero.
 * Variables have lifetimes like this, because we don't want to remove variables that are being used
 */
static void destroy_sysvar(sysvar_t* var)
{
    /* destroy_oss_obj clears obj->priv before calling it's child destructor */
    if (var->obj && var->obj->priv) {
        destroy_oss_obj(var->obj);
        return;
    }

    /* Release strduped key */
    kfree((void*)var->key);

    /* Just in case this was allocated on the heap (by strdup) */
    if (var->type == SYSVAR_TYPE_STRING && var->str_value)
        kfree((void*)var->str_value);

    destroy_atomic_ptr(var->refc);
    zfree_fixed(&__var_allocator, var);
}

static const char* __sysvar_fmt_key(char* str)
{
    u32 i = 0;

    while (str[i]) {
        if (str[i] >= 'a' && str[i] <= 'z')
            str[i] -= ('a' - 'A');
        else if (str[i] == ' ')
            str[i] = '_';
        i++;
    }
    return str;
}

/*!
 * @brief: Creates a profile variable
 *
 * This allocates the variable inside the variable zone allocator and sets its interlan value
 * equal to @value.
 * NOTE: this does not copy strings and allocate them on the heap!
 */
sysvar_t* create_sysvar(const char* key, uint16_t priv_lvl, enum SYSVAR_TYPE type, uint8_t flags, uintptr_t value)
{
    sysvar_t* var;

    var = zalloc_fixed(&__var_allocator);

    if (!var)
        return nullptr;

    memset(var, 0, sizeof *var);

    var->key = __sysvar_fmt_key(strdup(key));
    var->type = type;
    var->flags = flags;

    /* Placeholder value set */
    var->value = (type == SYSVAR_TYPE_STRING) ? strdup((const char*)value) : (void*)value;

    /*
     * Set the refcount to one in order to preserve the variable for multiple gets
     * and releases
     */
    var->refc = create_atomic_ptr_ex(1);
    /* Make sure type and value are already set */
    var->len = profile_var_get_size_for_type(var);
    var->obj = create_oss_obj_ex(key, priv_lvl);

    /* Register this sysvar to its object */
    oss_obj_register_child(var->obj, var, OSS_OBJ_TYPE_VAR, destroy_sysvar);

    return var;
}

sysvar_t* get_sysvar(sysvar_t* var)
{
    if (!var)
        return nullptr;

    uint64_t current_count = atomic_ptr_read(var->refc);
    atomic_ptr_write(var->refc, current_count + 1);

    return var;
}

void release_sysvar(sysvar_t* var)
{
    uint64_t current_count = atomic_ptr_read(var->refc);

    if (current_count--)
        atomic_ptr_write(var->refc, current_count);

    if (!current_count)
        destroy_sysvar(var);
}

bool sysvar_get_str_value(sysvar_t* var, const char** buffer)
{
    if (!buffer || !var || var->type != SYSVAR_TYPE_STRING)
        return false;

    *buffer = var->str_value;
    return true;
}

bool sysvar_get_qword_value(sysvar_t* var, uint64_t* buffer)
{
    if (!buffer || !var || var->type != SYSVAR_TYPE_QWORD)
        return false;

    *buffer = var->qword_value;
    return true;
}

bool sysvar_get_dword_value(sysvar_t* var, uint32_t* buffer)
{
    if (!buffer || !var || var->type != SYSVAR_TYPE_DWORD)
        return false;

    *buffer = var->dword_value;
    return true;
}

bool sysvar_get_word_value(sysvar_t* var, uint16_t* buffer)
{
    if (!buffer || !var || var->type != SYSVAR_TYPE_WORD)
        return false;

    *buffer = var->word_value;
    return true;
}

bool sysvar_get_byte_value(sysvar_t* var, uint8_t* buffer)
{
    if (!buffer || !var || var->type != SYSVAR_TYPE_BYTE)
        return false;

    *buffer = var->byte_value;
    return true;
}

/*!
 * @brief Write to the variables internal value
 *
 * NOTE: when dealing with strings in profile variables, we simply store a pointer
 * to a array of characters, but we never opperate on the string itself. This is left
 * to the caller. This means that when @var holds a pointer to a malloced string or
 * anything like that, the caller MUST free it before changing the profile var value,
 * otherwise this malloced string is lost to the void =/
 */
bool sysvar_write(sysvar_t* var, uint64_t value)
{
    if (!var)
        return false;

    /* Yikes */
    if ((var->flags & SYSVAR_FLAG_CONSTANT) == SYSVAR_FLAG_CONSTANT)
        return false;

    /* FIXME: can we infer a type here? */
    if (var->type == SYSVAR_TYPE_UNSET) {
        kernel_panic("FIXME: tried to write to an unset variable");
        return false;
    }

    switch (var->type) {
    case SYSVAR_TYPE_STRING:

        /*
         * Just in case the old value was strdupd
         * If this does not point to a valid heap object, this call is harmless
         */
        if (var->str_value)
            kfree((void*)var->str_value);

        var->str_value = strdup((const char*)value);
        break;
    case SYSVAR_TYPE_QWORD:
        var->qword_value = value;
        break;
    case SYSVAR_TYPE_DWORD:
        var->dword_value = (uint32_t)value;
        break;
    case SYSVAR_TYPE_WORD:
        var->word_value = (uint16_t)value;
        break;
    case SYSVAR_TYPE_BYTE:
        var->byte_value = (uint8_t)value;
        break;
    default:
        break;
    }

    return true;
}

int sysvar_read(sysvar_t* var, u8* buffer, size_t length)
{
    proc_t* current_proc;
    size_t data_size = NULL;
    size_t minimal_buffersize = 1;

    /* Get the current process */
    current_proc = get_current_proc();

    if (!current_proc)
        return -KERR_INVAL;

    switch (var->type) {
    case SYSVAR_TYPE_STRING:
        /* We need at least the null-byte */
        minimal_buffersize = 1;
        data_size = strlen(var->str_value) + 1;
        break;
    case SYSVAR_TYPE_QWORD:
        data_size = minimal_buffersize = sizeof(uint64_t);
        break;
    case SYSVAR_TYPE_DWORD:
        data_size = minimal_buffersize = sizeof(uint32_t);
        break;
    case SYSVAR_TYPE_WORD:
        data_size = minimal_buffersize = sizeof(uint16_t);
        break;
    case SYSVAR_TYPE_BYTE:
        data_size = minimal_buffersize = sizeof(uint8_t);
        break;
    default:
        break;
    }

    /* Check if we have enough space to store the value */
    if (!data_size || length < minimal_buffersize)
        return NULL;

    /* Make sure we are not going to copy too much into the user buffer */
    if (data_size > length)
        data_size = length;

    /*
     * TODO: move these raw memory opperations
     * to a controlled environment for safe copies and
     * 'memsets' into userspace
     */

    if (var->type == SYSVAR_TYPE_STRING) {
        /* Copy the string */
        memcpy(buffer, var->str_value, data_size);

        /* Make sure it's legal */
        buffer[length - 1] = '\0';
    } else
        memcpy(buffer, &var->value, data_size);

    return 0;
}

void init_sysvars(void)
{
    ASSERT(init_zone_allocator(&__var_allocator, (16 * Kib), sizeof(sysvar_t), NULL) == 0);
}

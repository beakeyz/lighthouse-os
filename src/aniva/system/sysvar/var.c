#include "var.h"
#include <libk/math/math.h>
#include "libk/flow/error.h"
#include "lightos/sysvar/shared.h"
#include "mem/heap.h"
#include "mem/zalloc/zalloc.h"
#include "oss/obj.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "system/profile/attr.h"
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
    case SYSVAR_TYPE_MEM_RANGE:
        return sizeof(page_range_t);
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

    switch (var->type) {
        case SYSVAR_TYPE_STRING:
            if (var->str_value)
                /* Just in case this was allocated on the heap (by strdup) */
                kfree((void*)var->str_value);
            break;
        case SYSVAR_TYPE_MEM_RANGE:
            if (var->range_value)
                /* We own this memory */
                kfree(var->range_value);
            break;
        default:
            break;
    }

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

static error_t __sysvar_allocate_value(sysvar_t* var, enum SYSVAR_TYPE type, void* buffer, size_t bsize, void** pvalue)
{
    void* ret;

    ASSERT(pvalue);

    if (sysvar_is_integer(type))
        memcpy(&ret, buffer, MIN(var->len, bsize));
    else {
        switch (type) {
            case SYSVAR_TYPE_STRING:
                ret = kmalloc(bsize);

                if (!ret)
                    return -ENOMEM;

                memcpy(ret, buffer, bsize);
                break;
            case SYSVAR_TYPE_MEM_RANGE:
                ret = kmalloc(sizeof(page_range_t));

                if (!ret)
                    return -ENOMEM;

                memcpy(ret, buffer, MIN(sizeof(page_range_t), bsize));
                break;
            default:
                return -ENOTSUP;
        }
    }

    *pvalue = ret;

    return 0;
}

/*!
 * @brief: Creates a profile variable
 *
 * @buffer: Contains the value for this sysvar of type @type and with size @bsize
 *          if bsize does not match the expected size of @type, the value is either
 *          truncated or only partially copied over. every sysvar owns their own
 *          buffer, which they also never give away. Values are always copied out
 *          of the buffer and mutated by mutate calls. This means we need notifiers
 *          (kevents) for sysvar changes, in case there are certain processes that
 *          require the most up-to-date value of any sysvar
 * @bsize: The size of @buffer
 */
sysvar_t* create_sysvar(const char* key, enum PROFILE_TYPE ptype, enum SYSVAR_TYPE type, uint8_t flags, void* buffer, size_t bsize)
{
    sysvar_t* var;

    var = zalloc_fixed(&__var_allocator);

    if (!var)
        return nullptr;

    memset(var, 0, sizeof *var);

    /* Make sure type and value are already set */
    var->len = profile_var_get_size_for_type(var);
    var->type = type;
    var->flags = flags;

    /* Placeholder value set */
    if (HAS_ERROR(__sysvar_allocate_value(var, type, buffer, bsize, &var->value))) {
        /* Free the var */
        zfree_fixed(&__var_allocator, var);
        return nullptr;
    }

    /* Allocate the rest */
    var->key = __sysvar_fmt_key(strdup(key));

    /*
     * Set the refcount to one in order to preserve the variable for multiple gets
     * and releases
     */
    var->refc = create_atomic_ptr_ex(1);
    var->obj = create_oss_obj_ex(key, ptype, NULL);

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

/*!
 * @brief: Simply read the most basic integer values from a sysvar
 */
u64 sysvar_read_u64(sysvar_t* var)
{
    if (!var || var->type != SYSVAR_TYPE_QWORD)
        return 0;

    return var->qword_value;
}

u32 sysvar_read_u32(sysvar_t* var)
{
    if (!var || var->type != SYSVAR_TYPE_DWORD)
        return 0;

    return var->dword_value;
}

u16 sysvar_read_u16(sysvar_t* var)
{
    if (!var || var->type != SYSVAR_TYPE_WORD)
        return 0;

    return var->word_value;
}

u8 sysvar_read_u8(sysvar_t* var)
{
    if (!var || var->type != SYSVAR_TYPE_BYTE)
        return 0;

    return var->byte_value;
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
error_t sysvar_write(sysvar_t* var, u8* buffer, size_t length)
{
    size_t write_sz;

    if (!var)
        return -EINVAL;

    /* Yikes */
    if ((var->flags & SYSVAR_FLAG_CONSTANT) == SYSVAR_FLAG_CONSTANT)
        return ECONST;

    /* FIXME: can we infer a type here? */
    if (var->type == SYSVAR_TYPE_UNSET) {
        kernel_panic("FIXME: tried to write to an unset variable");
        return -EINVAL;
    }

    /* Get the smallest of the two */
    write_sz = MIN(var->len, length);

    /* If this fucer is a basic type, we can just copy into the sysvars buffer itself */
    if (sysvar_is_integer(var->type))
        memcpy(&var->value, buffer, write_sz);
    else {
        /* Non-basic type.. we need to do fancy shit */
        switch (var->type) {
            case SYSVAR_TYPE_MEM_RANGE:
                /* Allocate our own page range if we don't have one yet */
                if (!var->range_value)
                    var->range_value = kmalloc(sizeof(page_range_t));

                /* Could not allocate =( sadge */
                if (!var->range_value)
                    return -ENOMEM;

                /* Copy, but make sure we don't overshoot the page range size */
                memcpy(var->range_value, buffer, MIN(write_sz, sizeof(page_range_t)));
                break;
            case SYSVAR_TYPE_STRING:
                // Forgor
            default:
                return -ENOTSUP;
        }
    }

    return 0;
}

error_t sysvar_read(sysvar_t* var, u8* buffer, size_t length)
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

error_t sysvar_sizeof(sysvar_t* var, size_t* psize)
{
    if (!var || !psize)
        return -EINVAL;

    /* Export the size */
    *psize = var->len;

    return 0;
}

void init_sysvars(void)
{
    ASSERT(init_zone_allocator(&__var_allocator, (16 * Kib), sizeof(sysvar_t), NULL) == 0);
}

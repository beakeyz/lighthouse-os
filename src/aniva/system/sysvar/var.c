#include "var.h"
#include "libk/flow/error.h"
#include "lightos/sysvar/shared.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/zalloc/zalloc.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "proc/env.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/profile/attr.h"
#include "system/sysvar/map.h"
#include <libk/math/math.h>
#include <libk/string.h>

static zone_allocator_t __var_allocator;

static uint32_t sysvar_get_size_for_type(enum SYSVAR_TYPE type, size_t bsize)
{
    switch (type) {
    case SYSVAR_TYPE_BYTE:
        return sizeof(uint8_t);
    case SYSVAR_TYPE_WORD:
        return sizeof(uint16_t);
    case SYSVAR_TYPE_DWORD:
        return sizeof(uint32_t);
    case SYSVAR_TYPE_QWORD:
        return sizeof(uint64_t);
    case SYSVAR_TYPE_MEM_RANGE:
        return sizeof(page_range_t);
    case SYSVAR_TYPE_STRING:
        return bsize + 1;
    default:
        break;
    }
    return 0;
}

static inline bool sysvar_is_valid_len(size_t len)
{
    return (len <= SYSVAR_MAX_LEN);
}

static inline error_t sysvar_set_len(sysvar_t* var, size_t len)
{
    if (!sysvar_is_valid_len(len))
        return -EINVAL;

    var->len = len;
    return 0;
}

void sysvar_lock(sysvar_t* var)
{
    mutex_lock(var->obj->lock);
}

void sysvar_unlock(sysvar_t* var)
{
    mutex_unlock(var->obj->lock);
}

/*!
 * @brief: Deallocates a variables memory and removes it from its profile if it still has one
 *
 * Should not be called directly, but only by release_profile_var once its refcount has dropped to zero.
 * Variables have lifetimes like this, because we don't want to remove variables that are being used
 */
static void destroy_sysvar(sysvar_t* var)
{
    destroy_oss_obj(var->obj);
}

/*!
 * Called by the destruction of the oss object
 */
static void __destroy_sysvar(sysvar_t* var)
{
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
    void* ret = NULL;
    size_t write_sz;

    if (!bsize)
        return 0;

    ASSERT(pvalue);

    write_sz = MIN(var->len, bsize);

    if (sysvar_is_integer(type))
        memcpy(&ret, buffer, write_sz);
    else {
        switch (type) {
        case SYSVAR_TYPE_STRING:
            ret = kmalloc(var->len);

            if (!ret)
                return -ENOMEM;

            memcpy(ret, buffer, write_sz);
            break;
        case SYSVAR_TYPE_MEM_RANGE:
            ret = kmalloc(sizeof(page_range_t));

            if (!ret)
                return -ENOMEM;

            memcpy(ret, buffer, write_sz);
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
sysvar_t* create_sysvar(const char* key, pattr_t* pattr, enum SYSVAR_TYPE type, uint8_t flags, void* buffer, size_t bsize)
{
    sysvar_t* var;

    /* Both bsize and buffer may be null, but if there is a size, we also need a buffer */
    if (!key || !sysvar_is_valid_len(bsize) || (!buffer && bsize))
        return nullptr;

    var = zalloc_fixed(&__var_allocator);

    if (!var)
        return nullptr;

    memset(var, 0, sizeof *var);

    /* Make sure type and value are already set */
    var->len = sysvar_get_size_for_type(type, bsize);
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

    KLOG_DBG("Creating sysvar: %s\n", var->key);

    /*
     * Set the refcount to one in order to preserve the variable for multiple gets
     * and releases
     */
    var->refc = 1;
    var->obj = create_oss_obj_ex(
        var->key,
        pattr ? pattr->ptype : PROFILE_TYPE_USER,
        pattr ? pattr->pflags : nullptr);

    /* Register this sysvar to its object */
    oss_obj_register_child(var->obj, var, OSS_OBJ_TYPE_VAR, __destroy_sysvar);

    return var;
}

penv_t* sysvar_get_parent_env(sysvar_t* var)
{
    penv_t* env = nullptr;
    oss_node_t* parent_node;

    /* Lock this fucker */
    sysvar_lock(var);

    /* Weird but ok */
    if (!var || !var->obj)
        goto unlock_and_return;

    /* Grab the parent node */
    parent_node = var->obj->parent;

    /* Check this */
    if (!oss_node_can_contain_sysvar(parent_node))
        goto unlock_and_return;

    /* Unwrap, since nodes with OSS_VMEM_NODE inherit their private field with their parent */
    env = (penv_t*)oss_node_unwrap(parent_node);

unlock_and_return:
    sysvar_unlock(var);
    return env;
}

sysvar_t* get_sysvar(sysvar_t* var)
{
    if (!var)
        return nullptr;

    sysvar_lock(var);

    var->refc++;

    sysvar_unlock(var);

    return var;
}

void release_sysvar(sysvar_t* var)
{
    u32 c_refc;

    sysvar_lock(var);

    c_refc = var->refc;

    if (c_refc)
        c_refc = c_refc - 1;

    if (!c_refc)
        return destroy_sysvar(var);

    var->refc = c_refc;

    sysvar_unlock(var);
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
 * @brief: Export the string reference of this variable
 *
 * Pretty fucking dangerous, which is why we require the sysvar to be locked when this is called
 */
const char* sysvar_read_str(sysvar_t* var)
{
    if (var->type != SYSVAR_TYPE_STRING || !mutex_is_locked_by_current_thread(var->obj->lock))
        return nullptr;

    return var->str_value;
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
error_t sysvar_write(sysvar_t* var, void* buffer, size_t length)
{
    error_t error;
    size_t write_sz;
    size_t old_sz;

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

    sysvar_lock(var);

    error = 0;
    /* Get the smallest of the two */
    write_sz = MIN(var->len, length);
    old_sz = var->len;

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

            /* Pre-set error */
            error = -ENOMEM;

            /* Could not allocate =( sadge */
            if (!var->range_value)
                goto unlock_and_exit;
            break;
        case SYSVAR_TYPE_STRING:
            error = -EINVAL;

            /* First, try to set the new length */
            if (sysvar_set_len(var, length + 1))
                goto unlock_and_exit;

            /*
             * Update write size.
             */
            write_sz = var->len;

            if (var->len > old_sz) {
                /* Free the previous buffer, if it exists and wasn't big enough */
                if (var->str_value)
                    kfree((void*)var->str_value);
                /* Allocate a new buffer */
                var->str_value = kmalloc(write_sz);
            }

            /* Pre-set error */
            error = -ENOMEM;

            /* FAILEDDD */
            if (!var->str_value)
                goto unlock_and_exit;

            /* Clear the entire range out */
            memset(var->value, 0, length + 1);
        default:
            error = -ENOTSUP;
            goto unlock_and_exit;
        }

        /* Copy, but make sure we don't overshoot size */
        memcpy(var->value, buffer, write_sz);
    }

    /* Reset the error */
    error = 0;

unlock_and_exit:
    sysvar_unlock(var);

    return error;
}

error_t sysvar_read(sysvar_t* var, void* buffer, size_t length)
{
    error_t error;
    proc_t* current_proc;
    size_t data_size = NULL;
    size_t minimal_buffersize = 1;

    if (!var)
        return -EINVAL;

    /* Get the current process */
    current_proc = get_current_proc();

    if (!current_proc)
        return -KERR_INVAL;

    sysvar_lock(var);

    error = -ENULL;

    switch (var->type) {
    case SYSVAR_TYPE_STRING:
        /* If there is no string buffer, we can't read */
        if (!var->str_value)
            goto unlock_and_exit;

        /* We need at least the null-byte */
        minimal_buffersize = 1;
        data_size = var->len;
        break;
    case SYSVAR_TYPE_MEM_RANGE:
        /* If there is no range, we can't read */
        if (!var->range_value)
            goto unlock_and_exit;

        data_size = minimal_buffersize = sizeof(page_range_t);
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
        error = -ENOTSUP;
        goto unlock_and_exit;
    }

    /* Next error might be invalid */
    error = -EINVAL;

    /* Check if we have enough space to store the value */
    if (!data_size || length < minimal_buffersize)
        goto unlock_and_exit;

    /* Make sure we are not going to copy too much into the user buffer */
    if (data_size > length)
        data_size = length;

    /*
     * TODO: move these raw memory opperations
     * to a controlled environment for safe copies and
     * 'memsets' into userspace
     */

    /* Check how we need to copy shit */
    if (!sysvar_is_integer(var->type)) {
        /* Copy the object from value */
        memcpy(buffer, var->value, data_size);
    } else
        memcpy(buffer, &var->value, data_size);

    /* Success, clear the error status */
    error = 0;
unlock_and_exit:
    sysvar_unlock(var);

    return error;
}

error_t sysvar_sizeof(sysvar_t* var, size_t* psize)
{
    if (!var || !psize)
        return -EINVAL;

    sysvar_lock(var);

    /* Export the size */
    *psize = var->len;

    sysvar_unlock(var);

    return 0;
}

void init_sysvars(void)
{
    ASSERT(init_zone_allocator(&__var_allocator, (16 * Kib), sizeof(sysvar_t), NULL) == 0);
}

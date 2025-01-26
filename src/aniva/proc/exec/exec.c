#include "exec.h"
#include "proc/exec/elf/elf.h"

static aniva_exec_method_t* __exec_methods = NULL;

/*!
 * @brief: Find a suitable execution method and try to execute @object
 *
 *
 */
error_t aniva_exec(oss_object_t* object, proc_t* target, u32 flags)
{
    error_t error = -ENOIMPL;

    /*
     * Loop over all methods and try to execute @object
     * All these methods should have the ->f_execute field set, otherwise sm
     * went wrong during register, or the field got corrupted
     */
    for (aniva_exec_method_t* method = __exec_methods; method && !IS_OK(error); method = method->next)
        error = method->f_execute(object, target);

    return error;
}

/*!
 * @brief: Try to execute @object using the exec method @method
 */
error_t aniva_exec_by(struct aniva_exec_method* method, oss_object_t* object, proc_t* target, u32 flags)
{
    if (!method)
        return -EINVAL;

    /* Try to execute */
    return method->f_execute(object, target);
}

/*!
 * @brief: Register an execution method to the kernel
 */
int register_exec_method(aniva_exec_method_t* method)
{
    aniva_exec_method_t** slot;

    if (!method || !method->f_execute)
        return -EINVAL;

    /* Get a method slot */
    slot = aniva_exec_method_get_slot(method->key);

    if (!slot)
        return -EINVAL;

    if (*slot)
        return -EDUPLICATE;

    /* Set this guy to null, just to be sure */
    method->next = nullptr;
    /* We've reached the end of the link at this point. Just stick it here */
    *slot = method;

    return 0;
}

/*!
 * @brief: Unegister an execution method from the kernel
 */
int unregister_exec_method(const char* key)
{
    aniva_exec_method_t** slot = aniva_exec_method_get_slot(key);

    if (!slot)
        return -EINVAL;

    if (!(*slot))
        return -ENOENT;

    /* Skip the link, as to exclude the execution method with @key as the key */
    *slot = (*slot)->next;
    return 0;
}

aniva_exec_method_t** aniva_exec_method_get_slot(const char* key)
{
    size_t key_len;
    aniva_exec_method_t** ret = &__exec_methods;

    if (!key)
        return nullptr;

    /* Compute the key length */
    key_len = strlen(key);

    if (!key_len)
        return nullptr;

    /* Walk until we find a free guy */
    while (*ret && strncmp(key, (*ret)->key, key_len) != 0)
        ret = &(*ret)->next;

    return ret;
}

aniva_exec_method_t* aniva_exec_method_get(const char* key)
{
    /* Try and find a slot for this key */
    aniva_exec_method_t** slot = aniva_exec_method_get_slot(key);

    /* Couldn't find shit */
    if (!slot)
        return nullptr;

    /* Return the value at slot */
    return *slot;
}

void init_aniva_execution()
{
    init_elf_exec_method();
}

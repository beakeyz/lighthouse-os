/* envir.c
 *
 * This file oversees all things going on with process environment. Process under lightos live in a certain environment, that should have more or
 * less the same structure every time. Userspace expects this from us and relies on that fact. There are certain objects connected to a process
 * objects, that simply need to exist. Otherwise userspace might shit itself if it encounters a situation it doesn't know how to handle. Things like
 * the WOH (W)orking (O)bject (H)older or certain stdio sysvars need to exist in order for libc to even start in some cases. A good libc and lightos
 * core implementation should be robust enough to work around these objects not being present, but still we should provide userspace with all the needed
 * components in order to function propperly
 */

#include "envir.h"
#include "lightos/api/objects.h"
#include "oss/object.h"
#include "proc/proc.h"

/* The woh doesn't provide any object operations */
static oss_object_ops_t __woh_ops = { NULL };

/*!
 * @brief: Initializes a basic process environment
 *
 * Objects that are added:
 *  - WOH: Working Object Holder, stores a single downstream connection to the current working object
 */
error_t init_process_envir(proc_t* proc)
{
    oss_object_t* woh;

    woh = create_oss_object(OBJECT_WO_HOLDER, OF_NO_DISCON, OT_GENERIC, &__woh_ops, NULL);

    if (!woh)
        return -ENOMEM;

    /* Set the woh object for the process */
    proc->envir.woh = woh;

    /* Connect the woh to the process */
    return oss_object_connect(proc->obj, woh);
}

void destroy_process_envir(proc_t* proc)
{
    /* Disconnect the woh object */
    oss_object_disconnect(proc->obj, proc->envir.woh);
}

/*!
 * @brief: Tries to connect a new working object to the processes woh
 *
 * If there already is a WO connected, this guy will fail
 */
error_t process_envir_connect_wo(struct proc* proc, struct oss_object* new_wo)
{
    oss_object_t* woh;

    if (!proc || !new_wo)
        return -EINVAL;

    woh = proc->envir.woh;

    /* Already has a connected wo =( */
    if (oss_object_get_nr_connections(woh) >= 2)
        return -EALREADY;

    return oss_object_connect(woh, new_wo);
}

error_t process_envir_disconnect_wo(struct proc* proc, struct oss_object* old_wo)
{
    oss_object_t* woh;

    if (!proc || !old_wo)
        return -EINVAL;

    woh = proc->envir.woh;

    /* Try to disconnect @old_wo */
    return oss_object_disconnect(woh, old_wo);
}

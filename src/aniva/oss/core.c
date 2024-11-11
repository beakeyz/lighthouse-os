#include "core.h"
#include "dev/disk/volume.h"
#include "fs/core.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/malloc.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "oss/path.h"
#include "sync/mutex.h"
#include "sys/types.h"
#include <libk/string.h>

static oss_node_t* _rootnode = nullptr;
static mutex_t* _core_lock = nullptr;

static struct oss_node* _oss_create_path_abs_locked(const char* path);
static struct oss_node* _oss_create_path_locked(struct oss_node* node, const char* path);
static int _oss_resolve_node_locked(const char* path, oss_node_t** out);

static inline int _oss_attach_rootnode_locked(struct oss_node* node);
static inline int _oss_detach_rootnode_locked(struct oss_node* node);

/*!
 * @brief: Quick routine to log the current state of the kernel heap
 *
 * TEMP: (TODO) remove this shit
 */
static inline void _oss_test_mem()
{
    memory_allocator_t alloc;

    kheap_copy_main_allocator(&alloc);
    KLOG_DBG(" (*) Got %lld/%lld bytes free\n", alloc.m_free_size, alloc.m_used_size + alloc.m_free_size);
}

/*!
 * @brief: Quick routine to test the capabilities and integrity of OSS
 *
 * Confirmations:
 *  - oss_resolve can creatre memory leak
 *  - leak is not in any string duplications, since changing the path does not change the
 *    amount of bytes leaked
 */
void oss_test()
{
    oss_obj_t* init_obj;

    _oss_test_mem();

    oss_resolve_obj_rel(nullptr, "Root/System/test.drv", &init_obj);
    destroy_oss_obj(init_obj);

    _oss_test_mem();

    oss_resolve_obj_rel(nullptr, "Root/System/test.drv", &init_obj);
    destroy_oss_obj(init_obj);

    _oss_test_mem();

    kernel_panic("End of oss_test");
}

/*!
 * @brief: Find the path entry in @fullpath that is pointed to by @idx
 */
static const char* _get_pathentry_at_idx(const char* fullpath, uint32_t idx)
{
    const char* path_idx = fullpath;
    const char* c_subentry_start = fullpath;

    // System/
    while (*path_idx) {

        if (*path_idx != '/')
            goto cycle;

        if (!idx) {

            /* This is fucked lmao */
            if (!(*c_subentry_start))
                break;

            return c_subentry_start;
        }

        idx--;

        /* Set the current subentry to the next segment */
        c_subentry_start = path_idx + 1;
    cycle:
        path_idx++;
    }

    /* If we haven't reached the end yet, something went wrong */
    if (idx)
        return nullptr;

    if (!(*c_subentry_start))
        return nullptr;

    return c_subentry_start;
}

static const char* _find_path_subentry_at(const char* path, uint32_t idx)
{
    uint32_t i;
    const char* ret = nullptr;
    char* path_cpy = strdup(path);
    char* tmp;

    /* Non-const in, non-const out =) */
    tmp = (char*)_get_pathentry_at_idx(path_cpy, idx);

    /* Fuck man */
    if (!tmp)
        goto free_and_exit;

    /* Trim off the next pathentry spacer */
    for (i = 0; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = NULL;
            break;
        }
    }

    if (!i)
        goto free_and_exit;

    ret = strdup(tmp);

free_and_exit:
    kfree((void*)path_cpy);
    return ret;
}

static oss_node_t* _get_rootnode(const char* name)
{
    oss_node_entry_t* entry;

    if (!name)
        return nullptr;

    entry = nullptr;

    if (oss_node_find(_rootnode, name, &entry))
        return nullptr;

    if (!entry || entry->type != OSS_ENTRY_NESTED_NODE)
        return nullptr;

    return entry->node;
}

static inline oss_node_t* _get_rootnode_from_path(const char* path, uint32_t* idx)
{
    oss_node_t* ret;
    const char* this_name = _find_path_subentry_at(path, 0);

    if (!this_name)
        return nullptr;

    ret = _get_rootnode(this_name);

    kfree((void*)this_name);

    if (ret)
        (*idx)++;

    return ret;
}

static inline int __try_query_entry(const char* path, u32 gen_idx, oss_node_t* gen_node, oss_obj_t** obj_out, oss_node_t** node_out)
{
    int error;
    const char* rela_path;

    /* There was an error, but we've passed a generation node, try to see if it has awnsers */
    error = oss_get_relative_path(path, gen_idx, &rela_path);

    if (error)
        return error;

    /* Query the node for the thing we're looking for */
    if (obj_out)
        error = oss_node_query(gen_node, rela_path, obj_out);
    else
        error = oss_node_query_node(gen_node, rela_path, node_out);

    return error;
}

static inline int __try_export_entry(oss_node_entry_t* entry, oss_node_t* node_entry, oss_obj_t** obj_out, oss_node_t** node_out)
{
    int error = 0;

    /* No error in the scan, see if we found the right thing */
    if (obj_out && entry && entry->type == OSS_ENTRY_OBJECT) {
        /* Add a reference to the object */
        oss_obj_ref(entry->obj);

        /* Export it */
        *obj_out = entry->obj;
    } else if (node_out && node_entry)
        *node_out = node_entry;
    else
        /* Probably a type mismatch */
        error = -KERR_NOT_FOUND;

    return error;
}

struct __oss_resolve_context {
    const char* path;
    int error;
    u32 obj_gen_idx;
    oss_node_t* obj_gen_node;
    oss_node_t* current_node;
    oss_node_t* start_node;
    oss_node_entry_t* current_entry;
};

static inline int __try_query_parent_node(struct __oss_resolve_context* ctx, oss_obj_t** obj_out, oss_node_t** node_out)
{
    int error;
    char* full_path = strdup(ctx->path);
    oss_node_t* gen_node = ctx->start_node;

    /* Walk down the parent list to find an upstream object gen node */
    while (gen_node && gen_node->type != OSS_OBJ_GEN_NODE) {
        char* _full_path = kmalloc(strlen(full_path) + 1 + strlen(gen_node->name));

        /* Format the new full path */
        sfmt(_full_path, "%s/%s", gen_node->name, full_path);

        /* Free the old full path */
        kfree(full_path);

        /* Set the new pointer */
        full_path = _full_path;

        gen_node = gen_node->parent;
    }

    /* This would be kinda oof */
    if (!gen_node) {
        kfree(full_path);
        return -KERR_NOT_FOUND;
    }

    if (obj_out)
        error = oss_node_query(gen_node, full_path, obj_out);
    else
        error = oss_node_query_node(gen_node, full_path, node_out);

    kfree(full_path);
    return error;
}

static inline int __handle_oss_entry_scan_result(struct __oss_resolve_context* ctx, oss_obj_t** obj_out, oss_node_t** node_out)
{
    if (!ctx)
        return -KERR_INVAL;

    /* An error without a generation node, fuck us bro */
    if (ctx->error && !ctx->obj_gen_node)
        return __try_query_parent_node(ctx, obj_out, node_out);

    if (ctx->error)
        /* There was an error, but we've passed a generation node, try to see if it has awnsers */
        return __try_query_entry(ctx->path, ctx->obj_gen_idx, ctx->obj_gen_node, obj_out, node_out);

    return __try_export_entry(ctx->current_entry, ctx->current_node, obj_out, node_out);
}

/*!
 * @brief: This function will check if there is a deeper object generation node responsible for @rel
 */
static int __oss_get_deeper_object_generator_node(oss_node_t* rel, const char* path, oss_node_t** pGenNode, const char** pRelPath)
{
    oss_node_t* ret = nullptr;
    const char* ret_path = nullptr;
    oss_node_t* walker;

    if (!rel || !path || !pGenNode || !pRelPath)
        return -1;

    walker = rel;

    /* Walk down the node chain */
    do {
        /* If this is a generation node, we've found our fucker */
        if (walker && walker->type == OSS_OBJ_GEN_NODE)
            ret = walker;

        walker = walker->parent;
    } while (!ret && walker);

    /* Didn't find shit =( */
    if (!ret)
        return -1;

    /* Now we need to figure out the new relative path */
    size_t rel_path_len = 0;
    size_t ret_name_len = strlen(ret->name);
    const char* rel_path = NULL;

    /* Yikes, would be bad */
    if (oss_node_get_path(rel, &rel_path) || !rel_path)
        return -1;

    rel_path_len = strlen(rel_path);

    /* Find the name of @ret here */
    for (u64 i = 0; i < rel_path_len; i++) {
        /* Make sure we don't scan over the length of the string */
        u32 scan_len = (rel_path_len - i) < ret_name_len ? (rel_path_len - i) : ret_name_len;

        /* Check if we have reached the name of our generator node */
        if (strncmp(&rel_path[i], ret->name, scan_len))
            continue;

        /* Fuck, how did this happen? */
        if (i + scan_len + 1 >= rel_path_len)
            break;

        /*
         * We did! &rel_path[i] is now equal to the start of ret->name
         * Do + 1 to account for the path seperator
         */
        ret_path = strdup(&rel_path[i + scan_len + 1]);
    }

    /* Fuck this memory */
    kfree((void*)rel_path);

    /* TODO: Stick path to the end of ret_path, in such a way that it's always a valid oss path */
    kernel_panic("TODO: __oss_get_deeper_object_generator_node");

    *pGenNode = ret;
    *pRelPath = ret_path;
    return 0;
}

/*!
 * @brief: Resolve an oss entry based on a path
 *
 * Loops over every subpath inside the path and tries to itteratively find a new entry every time
 *
 * NOTE: Either obj_out or node_out must be null, as to indicate which type of oss entry we're looking for
 */
static int __oss_resolve_node_entry_rel_locked(struct oss_node* rel, const char* path, struct oss_obj** obj_out, struct oss_node** node_out)
{
    int error;
    u32 obj_gen_idx;
    oss_node_t* obj_gen_node;
    const char* c_subpath;
    oss_node_t* c_node;
    oss_path_t oss_path;
    oss_node_entry_t* c_entry;
    struct __oss_resolve_context ctx = { 0 };

    if (!obj_out && !node_out)
        return -KERR_INVAL;

    ASSERT_MSG(mutex_is_locked_by_current_thread(_core_lock), "Tried to call __oss_resolve_node_entry_rel_locked without having locked oss");

    error = 0;
    c_node = rel;
    obj_gen_node = nullptr;
    obj_gen_idx = 0;

    /* No relative node. Fetch the rootnode */
    if (!c_node)
        c_node = _rootnode;
    else
        /* We don't really care if this fails lul */
        (void)__oss_get_deeper_object_generator_node(rel, path, &obj_gen_node, NULL);

    /* Parse the path into a vector */
    error = oss_parse_path(path, &oss_path);

    /* Fuck */
    if (error)
        return error;

    /* First subpath was a skip. Start from the root xD */
    if (oss_path.subpath_vec[0] && *oss_path.subpath_vec[0] == _OSS_PATH_SKIP)
        c_node = _rootnode;

    /* Loop over every subpath */
    for (u32 i = 0; i < oss_path.n_subpath; i++) {
        c_subpath = oss_path.subpath_vec[i];

        /* Skip a path that starts with a skip identifier */
        if (*c_subpath == _OSS_PATH_SKIP)
            continue;

        /* Go back a node */
        if (strncmp(c_subpath, "..", 2) == 0) {
            if (c_node)
                c_node = c_node->parent;

            continue;
        }

        /* Check this mofo */
        if (strncmp(c_subpath, ".", 1) == 0)
            continue;

        /* Cache the object generation node */
        if (c_node && c_node->type == OSS_OBJ_GEN_NODE && !obj_gen_node) {
            obj_gen_node = c_node;
            obj_gen_idx = i;
        }

        /* Clear */
        c_entry = nullptr;

        /* Try to find shit on this node */
        error = oss_node_find(c_node, c_subpath, &c_entry);

        /* Can't find shit on this node */
        if (error || !oss_node_entry_has_node(c_entry))
            break;

        c_node = c_entry->node;
    }

    /* Destroy the constructed path, which we don't need anymore */
    oss_destroy_path(&oss_path);

    /*
     * We've finished scanning the path. If there was an error this may mean a few things:
     * 1) Somewhere along the way there was a mismatch, we might terminate early
     * 2) There might have been an object where we might have expected a node
     */

    /* Pack the result of the scan into the context struct */
    ctx.path = path;
    ctx.error = error;
    ctx.start_node = rel;
    ctx.obj_gen_idx = obj_gen_idx;
    ctx.obj_gen_node = obj_gen_node;
    ctx.current_node = c_node;
    ctx.current_entry = c_entry;

    /* Pass the entire contex into the handler function */
    return __handle_oss_entry_scan_result(&ctx, obj_out, node_out);
}

/*!
 * @brief: Resolve a oss_object relative from an oss_node
 *
 * If @rel is null, we default to _local_root
 */
int oss_resolve_obj_rel(struct oss_node* rel, const char* path, struct oss_obj** out)
{
    int error;

    if (!out || !path)
        return -KERR_INVAL;

    *out = NULL;

    mutex_lock(_core_lock);

    error = __oss_resolve_node_entry_rel_locked(rel, path, out, NULL);

    mutex_unlock(_core_lock);

    return error;
}

/*!
 * @brief: Looks through the registered nodes and tries to resolve a oss_obj out of @path
 *
 * These paths are always absolute
 */
int oss_resolve_obj(const char* path, oss_obj_t** out)
{
    int error;

    if (!out || !path)
        return -1;

    *out = NULL;

    mutex_lock(_core_lock);

    error = __oss_resolve_node_entry_rel_locked(NULL, path, out, NULL);

    mutex_unlock(_core_lock);
    return error;
}

int oss_resolve_node_rel(struct oss_node* rel, const char* path, struct oss_node** out)
{
    int error;

    mutex_lock(_core_lock);

    error = __oss_resolve_node_entry_rel_locked(rel, path, NULL, out);

    mutex_unlock(_core_lock);

    return error;
}

static int _oss_resolve_node_locked(const char* path, oss_node_t** out)
{
    if (!out || !path)
        return -1;

    *out = NULL;

    return __oss_resolve_node_entry_rel_locked(_rootnode, path, NULL, out);
}

/*!
 * @brief: Looks through the registered nodes and tries to resolve a node out of @path
 */
int oss_resolve_node(const char* path, oss_node_t** out)
{
    int error;

    mutex_lock(_core_lock);

    error = _oss_resolve_node_locked(path, out);

    mutex_unlock(_core_lock);

    return error;
}

static struct oss_node* _oss_create_path_locked(struct oss_node* node, const char* path)
{
    int error;
    uint32_t c_idx;
    oss_node_t* c_node;
    oss_node_t* new_node;
    oss_node_entry_t* c_entry;
    const char* this_name;

    c_idx = 0;
    c_node = node;
    new_node = nullptr;

    if (!c_node) {
        c_node = _get_rootnode_from_path(path, &c_idx);

        if (!c_node)
            return nullptr;
    }

    while (*path && (this_name = _find_path_subentry_at(path, c_idx++))) {

        /* Find this entry */
        error = oss_node_find(c_node, this_name, &c_entry);

        /* We can't create a path if it points to an object */
        if (!error && c_entry->type == OSS_ENTRY_OBJECT)
            goto free_and_exit;

        /* Could not find this bitch, create a new node */
        if (error || !c_entry) {
            new_node = create_oss_node(this_name, OSS_OBJ_STORE_NODE, NULL, NULL);

            error = oss_node_add_node(c_node, new_node);

            if (error)
                goto free_and_exit;

            error = oss_node_find(c_node, this_name, &c_entry);

            /* Failed twice, this is bad lmao */
            if (error)
                goto free_and_exit;

            new_node = nullptr;
        }

        ASSERT_MSG(c_entry->type == OSS_ENTRY_NESTED_NODE, "Somehow created a non-nested node entry xD");

        kfree((void*)this_name);
        c_node = c_entry->node;

        continue;

    free_and_exit:
        if (new_node)
            destroy_oss_node(new_node);

        kfree((void*)this_name);
        return nullptr;
    }

    /* If we gracefully exit the loop, the creation succeeded */
    return c_node;
}

/*!
 * @brief: Creates a chain of nodes for @path
 *
 * @returns: The last node it created
 */
struct oss_node* oss_create_path(struct oss_node* node, const char* path)
{
    oss_node_t* ret;

    mutex_lock(_core_lock);

    ret = _oss_create_path_locked(node, path);

    mutex_unlock(_core_lock);

    return ret;
}

/*!
 * @brief: Internal function to call when the core lock is locked
 */
static struct oss_node* _oss_create_path_abs_locked(const char* path)
{
    const char* this_name;
    const char* rel_path;
    oss_node_t* c_node;

    if (!path)
        return nullptr;

    /* Find the root node name */
    this_name = _find_path_subentry_at(path, 0);

    /* Try to fetch the rootnode */
    c_node = _get_rootnode(this_name);

    /* Clean up */
    if (this_name)
        kfree((void*)this_name);

    /* Invalid path =/ */
    if (!c_node)
        return nullptr;

    rel_path = path;

    while (*rel_path) {
        if (*rel_path++ == '/')
            break;
    }

    return _oss_create_path_locked(c_node, rel_path);
}

/*!
 * @brief: Creates an absolute path
 */
struct oss_node* oss_create_path_abs(const char* path)
{
    oss_node_t* ret;

    mutex_lock(_core_lock);

    ret = _oss_create_path_abs_locked(path);

    mutex_unlock(_core_lock);

    return ret;
}

static inline int _oss_attach_rootnode_locked(struct oss_node* node)
{
    return oss_node_add_node(_rootnode, node);
}

int oss_attach_rootnode(struct oss_node* node)
{
    int error;

    if (!node)
        return -1;

    mutex_lock(_core_lock);

    error = _oss_attach_rootnode_locked(node);

    mutex_unlock(_core_lock);
    return error;
}

static inline int _oss_detach_rootnode_locked(struct oss_node* node)
{
    oss_node_entry_t* entry;

    if (oss_node_remove_entry(_rootnode, node->name, &entry))
        return -1;

    destroy_oss_node_entry(entry);
    return 0;
}

int oss_detach_rootnode(struct oss_node* node)
{
    int error;

    if (!node)
        return -1;

    mutex_lock(_core_lock);

    error = _oss_detach_rootnode_locked(node);

    mutex_unlock(_core_lock);
    return error;
}

/*!
 * @brief: Attach @node to @path
 *
 * There must be a node attached to the end of @path
 */
int oss_attach_node(const char* path, struct oss_node* node)
{
    int error;
    oss_node_t* c_node;

    if (!path)
        return -1;

    error = oss_resolve_node(path, &c_node);

    if (error)
        return error;

    mutex_lock(_core_lock);

    error = oss_node_add_node(c_node, node);

    mutex_unlock(_core_lock);
    return error;
}

static int _try_path_remove_obj_name(char* path, const char* name)
{
    size_t path_len;
    size_t name_len;
    char* embedded_name;

    if (!path || !name)
        return -1;

    path_len = strlen(path);
    name_len = strlen(name);

    if (!path_len || !name_len)
        return -1;

    /* Path can't contain name */
    if (strlen(path) < strlen(name))
        return -1;

    embedded_name = &path[path_len - name_len];

    /* Compare */
    if (strcmp(embedded_name, name) || path[path_len - name_len - 1] != '/')
        return -1;

    /* Remove the slash so we cut off the name at the end of the path */
    path[path_len - name_len - 1] = NULL;
    return 0;
}

/*!
 * @brief: Attach an object relative of another node
 */
int oss_attach_obj_rel(struct oss_node* rel, const char* path, struct oss_obj* obj)
{
    int error;
    oss_node_t* target_node;
    char* path_cpy;

    error = -1;
    /* Copy the path */
    path_cpy = strdup(path);

    /* Try to remove the name of the object, if it is present in the path */
    (void)_try_path_remove_obj_name(path_cpy, obj->name);

    /* Perpare a path */
    target_node = oss_create_path(rel, path_cpy);

    if (!target_node)
        goto dealloc_and_exit;

    /* Attach the object to the last node in the reated chain */
    error = oss_node_add_obj(target_node, obj);

dealloc_and_exit:
    kfree((void*)path_cpy);
    return error;
}

int oss_attach_obj(const char* path, struct oss_obj* obj)
{
    int error;
    oss_node_t* node;

    error = oss_resolve_node(path, &node);

    if (error)
        return error;

    mutex_lock(_core_lock);

    error = oss_node_add_obj(node, obj);

    mutex_unlock(_core_lock);
    return error;
}

/*!
 * @brief: Attach a filesystem with node name @rootname to @path
 *
 */
int oss_attach_fs(const char* path, const char* rootname, const char* fs, volume_t* device)
{
    int error;
    oss_node_t* node;
    oss_node_t* mountpoint_node;
    fs_type_t* fstype;

    /* Try to get the filsystem type we want to mount */
    fstype = get_fs_type(fs);

    if (!fstype)
        return -1;

    /* Let the filesystem driver generate a node for us */
    node = fstype->f_mount(fstype, rootname, device);

    if (!node)
        return -1;

    /* Lock the core */
    mutex_lock(_core_lock);

    error = -1;
    /* Create a path to mount on */
    mountpoint_node = _oss_create_path_abs_locked(path);

    /* No mountpoint node means we mount as a rootnode */
    if (!mountpoint_node)
        error = _oss_attach_rootnode_locked(node);
    else
        /* Add the generated node into the oss tree */
        error = oss_node_add_node(mountpoint_node, node);

    if (error)
        goto unmount_and_error;

    /* Mark the volume as systemized */
    device->flags |= VOLUME_FLAG_SYSTEMIZED;

    /* Unlock and return */
    mutex_unlock(_core_lock);
    return error;

unmount_and_error:
    fstype->f_unmount(fstype, node);
    destroy_oss_node(node);

    mutex_unlock(_core_lock);
    return error;
}

/*!
 * @brief: Detach a filesystem node from @path
 *
 * This does not destroy the oss node nor it's attached children
 *
 * FIXME: is this foolproof?
 */
int oss_detach_fs(const char* path, struct oss_node** out)
{
    int error;
    fs_oss_node_t* fs;
    oss_node_t* node;
    oss_node_t* parent;

    if (out)
        *out = NULL;

    mutex_lock(_core_lock);

    /* Find the node we want to detach */
    error = _oss_resolve_node_locked(path, &node);

    if (error)
        goto exit_and_unlock;

    if (node->type != OSS_OBJ_GEN_NODE)
        goto exit_and_unlock;

    fs = oss_node_getfs(node);

    /* Make sure the fs driver unmounts our bby */
    error = fs->m_type->f_unmount(fs->m_type, node);

    ASSERT_MSG(error == KERR_NONE, " ->f_unmount call failed");

    parent = node->parent;

    if (!parent)
        error = _oss_detach_rootnode_locked(node);
    else
        /* Remove the node from it's parent */
        error = oss_node_remove_entry(parent, node->name, NULL);

    if (error)
        goto exit_and_unlock;

    if (out)
        *out = node;

exit_and_unlock:
    mutex_unlock(_core_lock);
    return error;
}

/*!
 * @brief: Remove an object from it's path
 */
int oss_detach_obj(const char* path, struct oss_obj** out)
{
    kernel_panic("TODO: oss_detach_obj");
}

/*!
 * @brief: Remove an object from it's path, relative from @rel
 */
int oss_detach_obj_rel(struct oss_node* rel, const char* path, struct oss_obj** out)
{
    kernel_panic("TODO: oss_detach_obj_rel");
}

const char* oss_get_objname(const char* path)
{
    uint32_t c_idx;
    const char* this_name;
    const char* next_name;

    c_idx = 0;

    while ((this_name = _find_path_subentry_at(path, c_idx++))) {
        next_name = _find_path_subentry_at(path, c_idx);

        if (!next_name)
            break;

        kfree((void*)next_name);
        kfree((void*)this_name);
    }

    return this_name;
}

/*!
 * @brief: Create OSS root node registry
 */
void init_oss()
{
    init_oss_nodes();

    char rootname[] = { _OSS_PATH_SKIP, '\0' };

    _core_lock = create_mutex(NULL);
    _rootnode = create_oss_node(rootname, OSS_OBJ_STORE_NODE, nullptr, nullptr);
}

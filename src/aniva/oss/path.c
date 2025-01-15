#include "path.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include <libk/string.h>

int oss_parse_path(const char* path, oss_path_t* p_path)
{
    return oss_parse_path_ex(path, p_path, _OSS_PATH_SLASH);
}

static inline bool __should_count_subpath(const char* start_ptr, const char* end_ptr)
{
    return ((u64)(end_ptr - start_ptr) >= 1 && *start_ptr != _OSS_PATH_SKIP);
}

/*!
 * @brief: Function to parse a path into tokens
 *
 * @returns: A negative error code if the error had to with path integity
 */
int oss_parse_path_ex(const char* path, oss_path_t* p_path, char seperator)
{
    u32 n_subpath;
    u32 c_delta;
    /* Prepare the current pointer to one behind the path string, in order to fix the itteration */
    const char* c_char_ptr = path - 1;
    const char* delta_ptr = path;

    if (!path)
        return -KERR_INVAL;

    n_subpath = NULL;

    /* Scan the path */
    do {
        /* Next character */
        c_char_ptr++;

        /*
         * If we've reached a slash and our delta pointer is more than one byte
         * away from our main pointer, we've found a subpath
         */
        if (*c_char_ptr == seperator || *c_char_ptr == NULL) {

            if (__should_count_subpath(delta_ptr, c_char_ptr))
                n_subpath++;

            /* Update the delta pointer to our location */
            delta_ptr = c_char_ptr + 1;
        }

    } while (*c_char_ptr);

    /* No subpaths. exit */
    if (!n_subpath)
        return ENOENT;

    p_path->subpath_vec = kmalloc(n_subpath * sizeof(const char*));

    if (!p_path)
        return -KERR_NOMEM;

    /* Reset the pointers */
    n_subpath = 0;
    c_char_ptr = path - 1;
    delta_ptr = path;

    /* Scan the path again */
    do {
        /* Next character */
        c_char_ptr++;

        /*
         * If we've reached a slash and our delta pointer is more than one byte
         * away from our main pointer, we've found a subpath
         */
        if (*c_char_ptr == seperator || *c_char_ptr == NULL) {

            if (!__should_count_subpath(delta_ptr, c_char_ptr))
                goto skip_subpath;

            /* Compute the delta between the pointers */
            c_delta = (u32)(c_char_ptr - delta_ptr);

            /* Allocate space */
            p_path->subpath_vec[n_subpath] = kmalloc(c_delta + 1);

            /* Fuck, abort */
            if (!p_path->subpath_vec[n_subpath])
                break;

            /* Copy the subpath */
            strncpy(p_path->subpath_vec[n_subpath], delta_ptr, c_delta);

            /* Terminate the string */
            p_path->subpath_vec[n_subpath][c_delta] = NULL;

            /* Increment the counter */
            n_subpath++;

        skip_subpath:
            /* Update the delta pointer to our location */
            delta_ptr = c_char_ptr + 1;
        }

    } while (*c_char_ptr);

    /* Set the subpath count last */
    p_path->n_subpath = n_subpath;

    return 0;
}

int oss_get_relative_path(const char* path, u32 subpath_idx, const char** p_relpath)
{
    /* Prepare the current pointer to one behind the path string, in order to fix the itteration */
    const char* end_ptr = path - 1;
    const char* base_ptr = path;

    if (!path)
        return -KERR_INVAL;

    /* Scan the path */
    do {
        /* Next character */
        end_ptr++;

        /*
         * If we've reached a slash and our delta pointer is more than one byte
         * away from our main pointer, we've found a subpath
         */
        if (*end_ptr == _OSS_PATH_SLASH || *end_ptr == NULL) {

            if (!subpath_idx) {
                /* end of the thing, return the base pointer */
                *p_relpath = base_ptr;
                return 0;
            }

            /* Only decrement if we have one or more byte(s) in between the two pointers */
            if ((u64)(end_ptr - base_ptr) >= 1)
                subpath_idx--;

            /* Update the delta pointer to our location */
            base_ptr = end_ptr + 1;
        }

    } while (*end_ptr);

    return -KERR_NOT_FOUND;
}

int oss_destroy_path(oss_path_t* path)
{
    if (!path || !path->n_subpath)
        return 0;

    for (u32 i = 0; i < path->n_subpath; i++)
        kfree((void*)path->subpath_vec[i]);

    kfree(path->subpath_vec);
    return 0;
}

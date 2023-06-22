#ifndef __LIGHTENV_HANDLE__
#define __LIGHTENV_HANDLE__

/*
 * A handle is essensially an index into the table that the kernel
 * keeps track of for us. It contains all kinds of references to things
 * a user process can have access to, like files, processes, drivers,
 * structures, ect. A userprocess cant do anything with these handles themselves, but instead 
 * they can ask the kernel to do things on their behalf
 *
 * negative handles mean invalid handles of different types
 */
#include "LibSys/system.h"
#include "stdint.h"

typedef int handle_t;

#define HNDL_INV            (-1) /* Tried to get a handle from an invalid source */
#define HNDL_NOT_FOUND      (-2) /* Could not resolve the handle on the kernel side */
#define HNDL_BUSY           (-3) /* Handle target was busy and could not accept the handle at this time */
#define HNDL_TIMED_OUT      (-4) /* Handle target was not marked busy but still didn't accept the handle */
#define HNDL_NO_PERM        (-5) /* No permission to recieve a handle */
#define HNDL_PROTECTED      (-6) /* Handle target is protected and cant have handles right now */

/*
 * Check if a certain handle is actually valid for use
 */
BOOL verify_handle(
  __IN__ handle_t hndl
);

/*
 * Tell the kernel we want to expand our 
 * capacity for handles. We should pose a hard limit though
 * (default is around 512 per process)
 * When a process needs more than 1000 handles I think we can 
 * assume that they are doing something weird lol (unless that process is a service / driver )
 */
BOOL expand_handle_capacity(
  __IN__ size_t extra_size
);

#endif // !__LIGHTENV_HANDLE__

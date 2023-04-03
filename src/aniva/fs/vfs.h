// TODO: move this to src/fs and use certain architecture-specific shit 
// depending on the arch

#ifndef __ANIVA_VFS__
#define __ANIVA_VFS__

#include "dev/disk/generic.h"
#include "fs/namespace.h"
#include "fs/vnode.h"
#include "sync/mutex.h"
#include <libk/hive.h>

#define VFS_PATH_SEPERATOR '/'

struct vfs;

/*
 * The VFS consists of namespaces that in turn hold vnodes. Every vnode 
 * Represents a file, socket, FIFO, ect. Kernellevel sockets and vnodes 
 * that act like sockets are related, since under the hood, vnodes make use
 * of kernelsockets. 
 * When a filesystem gets mounted on the VFS, we initially just give it a 
 * namespace where it can throw child namespaces and vnodes at. When a mounted
 * object tries to access or change anything other than namespaces or vnodes under 
 * its assigned namespace, it will be denied and handled accordingly. 
 *
 * vnodes recieve pointers to their attached devices, so that any blockreads or writes
 * will move through the correct disk or manager (i.e. ramdisks get virtual disk
 * devices, which are just wrappers around RAM)
 *
 */
typedef struct vfs {
  char* m_id;

  mutex_t* m_lock;

  hive_t* m_namespaces;
} vfs_t;

void init_vfs();

/*
 * When mounting anything, we do that by giving the VFS information 
 * about what it is we are trying to mount. We can mount:
 *  - filesystems
 *  - symlinks
 *  - files
 *  - ect.
 *
 */
ErrorOrPtr vfs_mount(const char* path, vnode_t* node);
ErrorOrPtr vfs_unmount(const char* path);

/*
 * Resolve an absolute path
 */
vnode_t* vfs_resolve(const char* path);

ErrorOrPtr vfs_attach_namespace(const char* path);

ErrorOrPtr vfs_attach_root_namespace(vnamespace_t* namespace);

#endif // !__ANIVA_VFS__

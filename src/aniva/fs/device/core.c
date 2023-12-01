#include "dev/core.h"
#include "dev/device.h"
#include "dev/manifest.h"
#include "fs/vfs.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include <fs/vnode.h>
#include <fs/vobj.h>

static vnode_t* _das_node;

/*!
 * @brief: Message passing to devices
 *
 * Relays device messages to their appropriate driver
 */
static int dev_access_msg(vnode_t* node, dcc_t code, void* buffer, size_t size)
{
  return NULL;
}

/*!
 * @brief: Close a device on this vnode
 */
static int dev_access_close(vnode_t* node, vobj_t* obj)
{
  device_t* dev;

  dev = vobj_get_device(obj);

  /* Mismatch? */
  if (!dev || dev->obj != obj)
    return -1;

  dev->obj = nullptr;
  return 0;
}

/*!
 * @brief: Open a device on this vnode
 *
 * First two path parts should contain a path to the devices manifest, the last
 * one should be the driver itself. This means that this path may contain no more than 2 '/'
 * characters.
 *
 * This function relays on the fact that the caller has already verified that both the
 * variables (pointers) @node and @path.
 *
 * For example, the full path of a device on the efifb graphics driver would look like this:
 *  :/Dev/graphics/efifb/device
 */
static vobj_t* dev_access_open(vnode_t* node, char* path)
{
  vobj_t* ret;
  dev_manifest_t* manifest;
  device_t* device;
  uint64_t c_idx;
  uint8_t c_slash_count;
  char* path_cpy;

  const char* device_path;
  const char* manifest_path;

  if (!path || !(*path))
    return nullptr;

  path_cpy = strdup(path);

  if (!path_cpy)
    return nullptr;

  /*
   * TODO:
   * 1) Find the manifest
   * 2) Find the device
   */
  c_idx = 0;
  c_slash_count = 0;

  device_path = nullptr;
  manifest_path = path_cpy;

  println(path_cpy);

  /*
   * Try to parse our path to seperate the part that mentions our
   * driver manifest from the part that mentions the device
   */
  while (path_cpy[c_idx]) {

    if (path_cpy[c_idx] == VFS_PATH_SEPERATOR)
      c_slash_count++;

    /* Found our device path! */
    if (c_slash_count == 2) {
      /* Terminate the manifest path string */
      path_cpy[c_idx] = NULL;
      /* Set the device path */
      device_path = &path_cpy[c_idx+1];
      break;
    }

    c_idx++;
  }

  /* Path insufficient */
  if (c_slash_count != 2)
    goto dealloc_and_exit;

  /* Can we find a valid driver? */
  manifest = get_driver(manifest_path);

  if (!manifest)
    goto dealloc_and_exit;

  device = nullptr;

  /* Can we find a valid device? */
  if (manifest_find_device(manifest, &device, device_path) || !device)
    goto dealloc_and_exit;

  /* Create a generic vobj to hold our device reference */
  ret = create_generic_vobj(node, path);

  if (!ret)
    goto dealloc_and_exit;

  /* This vobj does not own this device, hence why it does not get a destructor */
  vobj_register_child(ret, device, VOBJ_TYPE_DEVICE, nullptr);

  /* Make sure the device knows about it's object */
  device->obj = ret;

  /* FIXME: What do we do when this fails?  */
  Must(vn_attach_object(node, ret));
  
  kfree(path_cpy);
  return ret;

dealloc_and_exit:
  kfree(path_cpy);
  return nullptr;
}

static struct generic_vnode_ops _das_node_ops = {
  .f_open = dev_access_open,
  .f_close = dev_access_close,
  .f_msg = dev_access_msg,
  .f_makedir = nullptr,
  .f_rmdir = nullptr,
  .f_force_sync = nullptr,
  .f_force_sync_once = nullptr,
};

static vnode_t* create_device_access_vnode()
{
  vnode_t* ret;

  ret = create_generic_vnode(VFS_DEFAULT_DEVICE_MP, VN_SYS);

  if (!ret)
    return nullptr;

  ret->m_type = VNODE_SYSTEM;
  ret->m_ops = &_das_node_ops;

  return ret;
}

/*!
 * @brief: Initialisation of the DAS
 *
 * Sets up the device vnode at :/Dev
 */
void init_dev_access()
{
  _das_node = create_device_access_vnode();

  ASSERT_MSG(_das_node, "Failed to create Device Access vnode");

  /* Mount a raw vnode on the default device mountpoint */
  Must(vfs_mount_raw(VFS_ROOT, _das_node));
}

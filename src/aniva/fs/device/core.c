#include "dev/core.h"
#include "dev/device.h"
#include "libk/flow/error.h"
#include <libk/string.h>
#include "oss/core.h"
#include "oss/node.h"

static oss_node_t* _das_node;

/*!
 * @brief: Message passing to devices
 *
 * Relays device messages to their appropriate driver
 */
static int dev_access_msg(oss_node_t* node, dcc_t code, void* buffer, size_t size)
{
  return NULL;
}

/*!
 * @brief: Close a device on this node
 */
static int dev_access_close(oss_node_t* node, oss_obj_t* obj)
{
  device_t* dev;

  dev = oss_obj_get_device(obj);

  /* Mismatch? */
  if (!dev || dev->obj != obj)
    return -1;

  dev->obj = nullptr;
  return 0;
}

/*!
 * @brief: Open a device on this node
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
static oss_obj_t* dev_access_open(oss_node_t* node, const char* path)
{
  kernel_panic("TODO: redo dev_access_open");
}

static int dev_access_destroy(oss_node_t* node)
{
  kernel_panic("TODO: dev_access_destroy");
}

static struct oss_node_ops _das_node_ops = {
  .f_open = dev_access_open,
  .f_close = dev_access_close,
  .f_msg = dev_access_msg,
  .f_destroy = dev_access_destroy,
};

static oss_node_t* create_device_access_node()
{
  oss_node_t* ret;

  ret = create_oss_node("Device", OSS_OBJ_STORE_NODE, &_das_node_ops, NULL);

  if (!ret)
    return nullptr;

  return ret;
}

/*!
 * @brief: Initialisation of the DAS
 *
 * Sets up the device node at :/Dev
 */
void init_dev_access()
{
  _das_node = create_device_access_node();

  ASSERT_MSG(_das_node, "Failed to create Device Access node");

  /* Mount a raw node on the default device mountpoint */
  ASSERT_MSG(!oss_attach_node(":", _das_node), "Failed to attach device access node");

}

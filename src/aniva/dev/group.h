#ifndef __ANIVA_DEV_GROUP__
#define __ANIVA_DEV_GROUP__

/*
 * Under aniva, devices are 'grouped' by the way we access them (By bus). 
 *
 * Groups are very static objects, since they get initialized once, after which they just kinda
 * exist on the system. They are in their most basic form just buckets to store devices of a specific
 * type
 *
 * Why use groups? Well we want to have an easy interface for drivers to create devices, where
 * they don't have to worry about if, how and where the device is exposed. By using groups, a driver
 * can simply call something like 'driver_register_device' on itself and our subsystems will then
 * be able to figure out where to put the device object.
 *
 * Groups are in charge of managing bus numbers and access to them. Groups can house both bus extentions
 * (Which are oss nodes) or raw devices (Which are oss objects).
 */

#include <libk/stddef.h>

struct device;
struct oss_node;

/* What type of group is this? */
enum DGROUP_TYPE {
  DGROUP_TYPE_NONE,
  DGROUP_TYPE_PCI,
  DGROUP_TYPE_AHCI,
  DGROUP_TYPE_IDE,
  DGROUP_TYPE_ISA,
  DGROUP_TYPE_USB,
  DGROUP_TYPE_GPIO,
  DGROUP_TYPE_I2C,
  DGROUP_TYPE_VIDEO,
  DGROUP_TYPE_ACPI,
  DGROUP_TYPE_MISC,
};

/* Hidden from userspace (And drivers?) */
#define DGROUP_FLAG_HIDDEN 0x00000001
#define DGROUP_FLAG_BUS    0x00000002

/*
 * dgroup is pretty much just a wrapper around an oss_node inside the device tree. We
 * just need an easy interface to interract with oss nodes that are of a group type.
 *
 * Locking is done through ->node
 */
typedef struct dgroup {
  /* Pointer to ->node->name (TODO: remove) */
  const char* name;

  uint32_t flags;
  enum DGROUP_TYPE type;

  void* priv;
  struct oss_node* node;
} dgroup_t;

void init_dgroups();

dgroup_t* register_dev_group(enum DGROUP_TYPE type, const char* name, uint32_t flags, struct oss_node* parent);
int unregister_dev_group(dgroup_t* group);

int dev_group_get(const char* name, dgroup_t** out);
int dev_group_getbus(dgroup_t* group, int busnum);

dgroup_t* dev_group_get_parent(dgroup_t* group);

int dev_group_get_device(dgroup_t* group, const char* path, struct device** dev);
int dev_group_add_device(dgroup_t* group, struct device* dev);
int dev_group_remove_device(dgroup_t* group, struct device* dev);

#endif // !__ANIVA_DEV_GROUP__

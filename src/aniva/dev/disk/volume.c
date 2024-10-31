#include "volume.h"
#include <oss/core.h>
#include <oss/node.h>

static oss_node_t* volumes_node;
static oss_node_t* volume_devices_node;

volume_t* create_volume(volume_id_t parent_id, volume_info_t* info, u32 flags);
void destroy_volume(volume_t* volume);

int register_volume(volume_t* volume);
int unregister_volume(volume_t* volume);

/* Volume opperation routines */
size_t volume_read(volume_t* volume, uintptr_t offset, void* buffer, size_t size);
size_t volume_write(volume_t* volume, uintptr_t offset, void* buffer, size_t size);
int volume_flush(volume_t* volume);
int volume_resize(volume_t* volume, uintptr_t new_min_blk, uintptr_t new_max_blk);
/* Completely removes a volume, also unregisters it and destroys it */
int volume_remove(volume_t* volume);

int register_volume_device(struct volume_device* volume_dev);
int unregister_volume_device(struct volume_device* volume_dev);

/*!
 * @brief: Initialize the volumes subsystem
 *
 * Organises volumes under %/Volumes and %/Dev/Volumes, where the former contains all volume objects and the latter contains any
 * volume carrier devices, which usually back any data associated with a volume.
 */
void init_volumes()
{
    volumes_node = create_oss_node("Volumes", OSS_OBJ_STORE_NODE, NULL, NULL);
    volume_devices_node = create_oss_node("Volumes", OSS_OBJ_STORE_NODE, NULL, NULL);

    /* Attach the nodes */
    ASSERT_MSG(oss_attach_node("%", volumes_node) == 0, "Failed to attach volumes node");
    ASSERT_MSG(oss_attach_node("%/Dev", volume_devices_node) == 0, "Failed to attach volume devices node");
}

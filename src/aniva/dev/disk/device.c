#include "device.h"

volume_device_t* create_volume_device(volume_info_t* info, volume_dev_ops_t* ops, u32 flags, void* private);
void destroy_volume_device(volume_device_t* volume_dev);

int volume_dev_set_info(volume_device_t* device, volume_info_t* pinfo);

int volume_dev_read(volume_device_t* volume_dev, u64 offset, void* buffer, size_t size);
int volume_dev_write(volume_device_t* volume_dev, u64 offset, void* buffer, size_t size);
int volume_dev_flush(volume_device_t* volume_dev);

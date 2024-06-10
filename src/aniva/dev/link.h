#ifndef __ANIVA_DEV_LINK__
#define __ANIVA_DEV_LINK__

struct oss_obj;
struct device;

/*
 * TODO: Implement
 *
 * A link is a oss object which simply points to a different device
 * in the device tree. We can use this to create a unified way for
 * userspace (or even kernelspace for that matter) to figure out which
 * device is currently actively used (or is responsible for certain things).
 * This way we can have seperate device probing + initialization and
 * serialization into the system process. Every devicetype subsystem
 * can implement it's own activate_device function, which enables a new
 * device and points its link to it, while disabeling its old device, if
 * it had one
 */
typedef struct device_link {
    struct oss_obj* obj;
    struct device* link;
} device_link_t;

#endif // !__ANIVA_DEV_LINK__

#ifndef __ANIVA_KDEV_CORE__
#define __ANIVA_KDEV_CORE__

#include "libk/flow/error.h"
#include <libk/stddef.h>

struct oss_obj;
struct oss_node;
struct aniva_driver;
struct drv_manifest;

#define DRIVER_URL_SEPERATOR '/'
#define DRIVER_TYPE_COUNT 9
#define DRIVER_WAIT_UNTIL_READY (size_t)-1

typedef uint8_t dev_type_t;

#define DT_DISK 0
#define DT_FS 1
#define DT_IO 2
#define DT_SOUND 3
#define DT_GRAPHICS 4
#define DT_SERVICE 5
#define DT_DIAGNOSTICS 6
#define DT_OTHER 7
#define DT_FIRMWARE 8

#define VALID_DEV_TYPE(type) ((type) && (type) < DRIVER_TYPE_COUNT)

typedef enum dev_flags {
  STANDARD = (1 << 0),
  FROM_FILE = (1 << 1),
  LOADED_WITH_WARNING = (1 << 2),
} DEV_FLAGS;

typedef struct dev_constraint {
  uint32_t max_active;
  uint32_t current_active;
  uint32_t max_count;
  uint32_t current_count;
  dev_type_t type;

  /* The rest of the qword taken by ->type -_- */
  uint8_t res0;
  uint16_t res1;
  uint32_t res2;

  /* This field should only be used when max_active is 1 and current_active is 1 */
  struct drv_manifest* active;
  /* The core driver for this driver type */
  struct drv_manifest* core;
} dev_constraint_t;

#define DRV_INFINITE            0xFFFFFFFF
#define DRV_SERVICE_MAX         0x100
#define drv_manifest_SOFTMAX    512

/* Defined in dev/core.c */
extern const char* dev_type_urls[DRIVER_TYPE_COUNT];

/*
 * Used by drivers to communicate, but also to send generic 
 * messages through sockets (i.e. some ccs are global, meaning they
 * are intercepted by the socket message dispatcher and interpreted)
 */
typedef const char*                     dev_url_t;

typedef uint32_t                        driver_control_code_t;

/* Welcome to the */
typedef driver_control_code_t           dcc_t;

/* Global driver control codes */
#define DCC_DATA                        (uint32_t)(0)
#define DCC_EXIT                        (uint32_t)(-1)
#define DCC_RESPONSE                    (uint32_t)(-2)
#define DCC_WRITE                       (uint32_t)(-3)

#define MAX_DRIVER_NAME_LENGTH          128
#define MAX_DRIVER_DESCRIPTOR_LENGTH    64  /* Descriptions should be short, simple and effective */

#define SOCKET_VERIFY_RESPONSE_SIZE(size) ((size) != ((size_t)-1))

#define EXPORT_DRIVER_PTR(name) USED SECTION(".kpcdrvs") aniva_driver_t* exported_##name = (aniva_driver_t*)&name
#define EXPORT_CORE_DRIVER(name) USED SECTION(".core_drvs") aniva_driver_t* exported_core_##name = (aniva_driver_t*)&name
#define EXPORT_DRIVER(name) USED SECTION(".expdrv") ALIGN(8) aniva_driver_t name
#define EXPORT_DEPENDENCIES(deps) USED SECTION(".deps") ALIGN(8) drv_dependency_t (deps)[]

#define EXPSYM_SHDR_NAME ".expsym"
#define EXPORT_DRVSYM USED SECTION(EXPSYM_SHDR_NAME)

#define NO_MANIFEST NULL

/*
 * Initializes the registry for kernel drivers. 
 * Also bootstraps any drivers that came precompiled 
 * with the kernel
 */
void init_aniva_driver_registry();
void init_driver_subsys();

// TODO: load driver from file

struct drv_manifest* allocate_dmanifest();

void free_dmanifest(struct drv_manifest* manifest);

/*
 * Registers the driver so it can be loaded
 */
ErrorOrPtr install_driver(struct drv_manifest* driver);

/*
 * Unregisters the driver and also unloads it if it is still loaded
 */
ErrorOrPtr uninstall_driver(struct drv_manifest* handle);

/*
 * load a driver from its structure in RAM
 */
ErrorOrPtr load_driver(struct drv_manifest* manifest);

/*
 * unload a driver from its structure in RAM
 */
ErrorOrPtr unload_driver(dev_url_t url);

/*
 * Check if the driver is installed into the grid
 */
bool is_driver_installed(struct drv_manifest* handle);

/*
 * Check if the driver is loaded and currently active
 */
bool is_driver_loaded(struct drv_manifest* handle);

/*
 * Find the handle to a driver through its url
 */
struct drv_manifest* get_driver(dev_url_t url);
struct drv_manifest* get_main_driver_from_type(dev_type_t type);
struct drv_manifest* get_driver_from_type(dev_type_t type, uint32_t index);
struct drv_manifest* get_core_driver(dev_type_t type);
int get_main_driver_path(char buffer[128], dev_type_t type);
size_t get_driver_type_count(dev_type_t type);

ErrorOrPtr foreach_driver(bool (*callback)(struct oss_node* h, struct oss_obj* obj, void* arg), void* arg);

int set_main_driver(struct drv_manifest* dev, dev_type_t type);
bool verify_driver(struct drv_manifest* manifest);

void replace_main_driver(struct drv_manifest* manifest, bool uninstall);

int register_core_driver(struct aniva_driver* driver, dev_type_t type);
int unregister_core_driver(struct aniva_driver* driver);

/*
 * This tries to get a driver from the loaded pool. If it is not
 * found, we see if it is installed and if it allows for dynamic 
 * loading. If the DRV_ALLOW_DYNAMIC_LOADING bit is set in both
 * the driver and the method, we try to load the driver anyway
 */
struct drv_manifest* try_driver_get(struct aniva_driver* driver, uint32_t flags);

/*
 * Marks the driver as ready to recieve packets preemptively
 */
ErrorOrPtr driver_set_ready(const char* path);

const char* driver_get_type_str(struct drv_manifest* driver);

/*
 * Find the url for a certain dev_type
 */
static ALWAYS_INLINE const char* get_driver_type_url(dev_type_t type) 
{
  return dev_type_urls[type];
}

ErrorOrPtr driver_send_msg(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size);
ErrorOrPtr driver_send_msg_a(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t resp_buffer_size);
ErrorOrPtr driver_send_msg_ex(struct drv_manifest* driver, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t resp_buffer_size);
ErrorOrPtr driver_send_msg_sync(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size);

int driver_map(struct drv_manifest* driver, void* base, size_t size, uint32_t page_flags);
int driver_unmap(struct drv_manifest* driver, void* base, size_t size);
void* driver_allocate(struct drv_manifest* driver, size_t size, uint32_t page_flags);
int driver_deallocate(struct drv_manifest* driver, void* base);
void* driver_kmalloc(struct drv_manifest* driver, size_t size);
int driver_kfree(struct drv_manifest* driver, void*);

/*
 * Sends a packet to the target driver and waits a number of scheduler yields for the socket to be ready
 * if the timer runs out before the socket is ready, we return nullptr
 * 
 * When mto is set to DRIVER_WAIT_UNTIL_READY, we simply wait untill the driver marks itself ready
 */
ErrorOrPtr driver_send_msg_sync_with_timeout(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, size_t mto);

extern const char* get_driver_url(struct aniva_driver* handle);
extern size_t get_driver_url_length(struct aniva_driver* handle);

#define DRIVER_VERSION(major, minor, bmp) {.maj = major, .min = minor, .bump = bmp} 

#endif //__ANIVA_KDEV_CORE__

#ifndef __ANIVA_KDEV_CORE__
#define __ANIVA_KDEV_CORE__

#include "libk/flow/error.h"
#include <libk/stddef.h>

struct oss_obj;
struct oss_node;
struct aniva_driver;
struct driver;

#define DRIVER_URL_SEPERATOR '/'
#define DRIVER_WAIT_UNTIL_READY (size_t) - 1
#define DRIVER_MAX_DEV_COUNT 64

enum DRIVER_TYPE {
    DT_DISK,
    DT_FS,
    DT_IO,
    DT_SOUND,
    DT_GRAPHICS,
    DT_SERVICE,
    DT_DIAGNOSTICS,
    DT_OTHER,
    DT_FIRMWARE,

    DRIVER_TYPE_COUNT,
};

#define VALID_DEV_TYPE(type) ((type) < DRIVER_TYPE_COUNT)

typedef enum dev_flags {
    STANDARD = (1 << 0),
    FROM_FILE = (1 << 1),
    LOADED_WITH_WARNING = (1 << 2),
} DEV_FLAGS;

/*!
 * @brief: Struct describing the constraints surrounding a single driver type
 *
 * TODO: Make these constraints accessable to userspace through sysconfig
 */
typedef struct dev_constraint {
    uint32_t max_active;
    uint32_t current_active;
    uint32_t max_count;
    uint32_t current_count;
    enum DRIVER_TYPE type;

    /* The rest of the qword taken by ->type -_- */
    uint8_t res0;
    uint16_t res1;
    uint32_t res2;
} dev_constraint_t;

#define DRV_INFINITE 0xFFFFFFFF
#define DRV_SERVICE_MAX 0x100
#define driver_SOFTMAX 512

/* Defined in dev/core.c */
extern const char* dev_type_urls[DRIVER_TYPE_COUNT];

/*
 * Used by drivers to communicate, but also to send generic
 * messages through sockets (i.e. some ccs are global, meaning they
 * are intercepted by the socket message dispatcher and interpreted)
 */
typedef const char* dev_url_t;

typedef uint32_t driver_control_code_t;

/* Welcome to the */
typedef driver_control_code_t dcc_t;

/* Global driver control codes */
#define DCC_DATA (uint32_t)(0)
#define DCC_EXIT (uint32_t)(-1)
#define DCC_RESPONSE (uint32_t)(-2)
#define DCC_WRITE (uint32_t)(-3)

#define MAX_DRIVER_NAME_LENGTH 128
#define MAX_DRIVER_DESCRIPTOR_LENGTH 64 /* Descriptions should be short, simple and effective */

#define SOCKET_VERIFY_RESPONSE_SIZE(size) ((size) != ((size_t) - 1))

#define EXPORT_DRIVER_PTR(name) USED SECTION(".kpcdrvs") aniva_driver_t* exported_##name = (aniva_driver_t*)&name
#define EXPORT_CORE_DRIVER(name) USED SECTION(".core_drvs") aniva_driver_t* exported_core_##name = (aniva_driver_t*)&name
#define EXPORT_DRIVER(name) USED SECTION(".expdrv") ALIGN(8) aniva_driver_t name
#define EXPORT_DEPENDENCIES(deps) USED SECTION(".deps") ALIGN(8) drv_dependency_t(deps)[]
#define EXPORT_EMPTY_DEPENDENCIES(deps) USED SECTION(".deps") ALIGN(8) drv_dependency_t(deps)[] = { \
    DRV_DEP_END,                                                                                    \
}

#define EXPSYM_SHDR_NAME ".expsym"
#define EXPORT_DRVSYM USED SECTION(EXPSYM_SHDR_NAME)

/*
 * Initializes the registry for kernel drivers.
 * Also bootstraps any drivers that came precompiled
 * with the kernel
 */
void init_aniva_driver_registry();
void init_driver_subsys();

// TODO: load driver from file

struct driver* allocate_ddriver();

void free_ddriver(struct driver* driver);

/*
 * Registers the driver so it can be loaded
 */
kerror_t install_driver(struct driver* driver);

/*
 * Unregisters the driver and also unloads it if it is still loaded
 */
kerror_t uninstall_driver(struct driver* handle);

/*
 * load a driver from its structure in RAM
 */
kerror_t load_driver(struct driver* driver);

/*
 * unload a driver from its structure in RAM
 */
kerror_t unload_driver(dev_url_t url);

/*
 * Check if the driver is installed into the grid
 */
bool is_driver_installed(struct driver* handle);

/*
 * Check if the driver is loaded and currently active
 */
bool is_driver_loaded(struct driver* handle);

/*
 * Find the handle to a driver through its url
 */
struct driver* get_driver(dev_url_t url);
struct driver* get_driver_from_type(enum DRIVER_TYPE type, uint32_t index);
size_t get_driver_type_count(enum DRIVER_TYPE type);
kerror_t foreach_driver(bool (*callback)(struct oss_node* h, struct oss_obj* obj, void* arg), void* arg);
bool verify_driver(struct driver* driver);

/*
 * This tries to get a driver from the loaded pool. If it is not
 * found, we see if it is installed and if it allows for dynamic
 * loading. If the DRV_ALLOW_DYNAMIC_LOADING bit is set in both
 * the driver and the method, we try to load the driver anyway
 */
struct driver* try_driver_get(struct aniva_driver* driver, uint32_t flags);

/*
 * Marks the driver as ready to recieve packets preemptively
 */
kerror_t driver_set_ready(const char* path);

const char* driver_get_type_str(struct driver* driver);

/*
 * Find the url for a certain dev_type
 */
static ALWAYS_INLINE const char* get_driver_type_url(enum DRIVER_TYPE type)
{
    return dev_type_urls[type];
}

kerror_t driver_send_msg(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size);
kerror_t driver_send_msg_a(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t resp_buffer_size);
kerror_t driver_send_msg_ex(struct driver* driver, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t resp_buffer_size);
kerror_t driver_send_msg_sync(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size);

int driver_map(struct driver* driver, void* base, size_t size, uint32_t page_flags);
int driver_unmap(struct driver* driver, void* base, size_t size);
void* driver_allocate(struct driver* driver, size_t size, uint32_t page_flags);
int driver_deallocate(struct driver* driver, void* base);
void* driver_kmalloc(struct driver* driver, size_t size);
int driver_kfree(struct driver* driver, void*);

/*
 * Sends a packet to the target driver and waits a number of scheduler yields for the socket to be ready
 * if the timer runs out before the socket is ready, we return nullptr
 *
 * When mto is set to DRIVER_WAIT_UNTIL_READY, we simply wait untill the driver marks itself ready
 */
kerror_t driver_send_msg_sync_with_timeout(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, size_t mto);

extern const char* get_driver_url(struct aniva_driver* handle);
extern size_t get_driver_url_length(struct aniva_driver* handle);

#define DRIVER_VERSION(major, minor, bmp)       \
    {                                           \
        .maj = major, .min = minor, .bump = bmp \
    }

#endif //__ANIVA_KDEV_CORE__

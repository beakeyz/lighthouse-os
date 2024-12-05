#ifndef __LIGHTOS_MODULES_H__
#define __LIGHTOS_MODULES_H__

typedef struct modules_version {
    unsigned char major;
    unsigned char minor;
    unsigned char bump;
    unsigned char patch;
} modules_version_t;

#define MODULES_VERSION(maj, min, bump, patch) \
    (modules_version_t) { (maj), (min), (bump), (patch) }

extern modules_version_t g_modules_version;

extern modules_version_t get_modules_version();

#endif // !__LIGHTOS_MODULES_H__

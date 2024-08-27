#ifndef __ANIVA_DRIVER_CONFIG_VARS_H__
#define __ANIVA_DRIVER_CONFIG_VARS_H__

#include <libk/stddef.h>
#include <oss/node.h>

struct drv_manifest;

/*
 * This header defines the framework for drivers (and devices) to expose a configuration
 * directory, which contain sysvars that are sensitive to state changes (i.e. they trigger
 * kevents when changed). This way driver settings can be changed on the fly.
 *
 * When the driver refuses the requested configuration change (like when a requested buffersize
 * is too small), the event is error-marked, and the sysvar isn't mutated. The caller should be
 * notified in this case (TODO: how).
 */

/*!
 * @brief: Function prototype for a drvconfig mutate callback
 *
 * This function is called when a thread tries to change a drivers configuration variable
 *
 * @driver: The driver object this variable works on
 * @var: The variable in question
 * @newval_var: A pseudo-image of what @var would look like after the mutate
 *
 * @returns: 0 if the driver is okay with the change and has processed it correctly and a negative error number
 * otherwise, if the mutation should be aborted.
 */
typedef int (*f_drvconfig_cb)(struct drv_manifest* driver, sysvar_t* var, sysvar_t* newval_var);

/* Driver configuration node */
typedef oss_node_t drvconf_node_t;

int drvconf_create(drvconf_node_t** pnode, struct drv_manifest* driver);
int drvconf_destroy(drvconf_node_t* drvconf_node);

int drvconf_add_num(drvconf_node_t* confnode, const char* key, u64 num);
int drvconf_add_str(drvconf_node_t* confnode, const char* key, const char* str);

int drvconf_remove(drvconf_node_t* confnode, const char* key);

#endif // !__ANIVA_DRIVER_CONFIG_VARS_H__

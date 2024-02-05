#ifndef __ANIVA_DEVICE_ENDPOINT__
#define __ANIVA_DEVICE_ENDPOINT__

/*
 * Define a generic device endpoint
 *
 * Devices can have multiple endpoints attached to them. It is up to the device driver to implement these endpoints.
 * These act as 'portals' between specific devices handled by a device driver and the software that wants to make use
 * of the device. Code in this header will therefore be directed at driver code, while any code that deals with the 
 * useage-side of devices should be put in user.h
 *
 * Functions here should be used by device drivers
 * Functions in user.h should be used by the rest of the system to (indirectly) invoke device services
 *
 * NOTE: user.h is a collection of shit that has to do with driver/device access from any outside source. This means
 * that when driver want to use other driver or devices they also interact with user.h
 */

#endif // !__ANIVA_DEVICE_ENDPOINT__

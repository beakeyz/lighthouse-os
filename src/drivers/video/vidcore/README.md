# Video core driver

This is the high-level driver for anything to do with video

Most of the time, there is only one graphics adapter present on a given system. It is our job to make sure
we have the correct driver loaded for this device. In the case that there are multiple graphics adapters or displays,
it is our job to make sure the correct one(s) are/is used

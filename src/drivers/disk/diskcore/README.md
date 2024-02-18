# Disk core driver

This is the high level disk driver that manages lower level device drivers for certain disk devices.
There are two types of device drivers for disks:

1) The controller driver (This could for example be NVMe, AHCI, IDE, ect.)
2) The harddisk driver

We only really manage the controller drivers (?)

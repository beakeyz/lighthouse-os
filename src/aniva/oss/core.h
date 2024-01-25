#ifndef __ANIVA_OSS_CORE__
#define __ANIVA_OSS_CORE__

/*
 * Core include file for the aniva OSS
 *
 * This is where we expose most of the OSS API to the rest of the system
 * things like filesystem drivers may make use of all the oss functions in order to
 * inject their services into the oss core. These drivers must create a node that can
 * catch external filesystem calls and translate them into filesystem language
 *
 * Let's take a generic FAT32 filesystem driver for example
 * (1) When it's loaded, it registers a new fs_type so the system knows we support this kind of fs.
 * Then, (2) when a subsystem finds a partition that might contain a valid FAT32 filesystem, it contacts
 * the oss API and asks it if it can attach the filesystem on this partition to the oss node tree.
 * (3) The oss will envoke the driver and the driver then has to comply to the mount call. It is only 
 * given information that is relevant to the driver at mount time. The only thing the driver needs to do
 * is do it's own fs-specific things to set up the filesystem and then create an oss node that can keep track
 * of all that shit. (4) OSS will verify that the returned oss node is OK and it will continue to attach it where
 * it needs to go. (5) Let's say that the filesystem gets attached to
 * ':/Root'
 * And there is a subsystem that calls the oss API to resolve a vobj at the path:
 * ':/Root/Some/file.txt'
 * When OSS recieves this call it will bump into the oss_node at :/Root and it discovers that it's a vobj-generator.
 * At this point it can call the oss_node opperation to resolve a vobj and it returns the vobj to the calling subsystem
 */

void init_oss();

int oss_resolve();
int oss_close();
int oss_move();
int oss_remove();

int oss_attach_node();
int oss_detach_node();
int oss_rename_node();

#endif // !__ANIVA_OSS_CORE__

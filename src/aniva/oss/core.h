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
 * And there is a subsystem that calls the oss API to resolve a oss_obj at the path:
 * ':/Root/Some/file.txt'
 * When OSS recieves this call it will bump into the oss_node at :/Root and it discovers that it's a oss_obj-generator.
 * At this point it can call the oss_node opperation to resolve a oss_obj and it returns the oss_obj to the calling subsystem
 */

struct oss_obj;
struct oss_node;
struct partitioned_disk_dev;

void init_oss();
void oss_test();

struct oss_node* oss_create_path(struct oss_node* node, const char* path);
struct oss_node* oss_create_path_abs(const char* path);

int oss_attach_rootnode(struct oss_node* node);
int oss_attach_node(const char* path, struct oss_node* node);
int oss_attach_obj(const char* path, struct oss_obj* obj);
int oss_attach_obj_rel(struct oss_node* rel, const char* path, struct oss_obj* obj);
int oss_attach_fs(const char* path, const char* rootname, const char* fs, struct partitioned_disk_dev* device);

int oss_detach_fs(const char* path, struct oss_node** out);
int oss_detach_obj(const char* path, struct oss_obj** out);
int oss_detach_obj_rel(struct oss_node* rel, const char* path, struct oss_obj** out);

int oss_resolve_node_rel(struct oss_node* rel, const char* path, struct oss_node** out);
int oss_resolve_node(const char* path, struct oss_node** out);

int oss_resolve_obj_rel(struct oss_node* rel, const char* path, struct oss_obj** out);
int oss_resolve_obj(const char* path, struct oss_obj** out);

const char* oss_get_objname(const char* path);

#endif // !__ANIVA_OSS_CORE__

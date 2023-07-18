#ifndef __ANIVA_DRV_LOADER__
#define __ANIVA_DRV_LOADER__

/*
 * The Aniva driver/library loader
 *
 * This submodule of the kernel is designated to load any external drivers that the kernel 
 * may need. The drivers that NEED to be loaded will be listed in a file that we either put in the 
 * kernels boot directory when the system is installed, or in the ramdisk. This file will list the paths 
 * of files that will be used to load the drivers. Every driver will be packed in either an ELF or a 
 * custom file format, when we have a compiler, linker and packer for our system. For now we will simply use ELF.
 * 
 * Every file can host multiple drivers and the ELF must contain certain sections that in turn contain the 
 * driver structures that the kernel uses to load the drivers. It will Scan these sections in order to identify
 * and verify all the drivers and them load them in order. 
 *
 * Since drivers will use kernel functionality directly, the loader will load an aditional file next to the drivers that
 * contain the kernel symbols, so that we can match any missing symbols in the GOT against our kernel.
 *
 * This loader is also capable to load certain libraries on behalf of any process and linking it at runtime against
 * this process. Some may argue that that is a job for a different userspace process, but to that I say: Fuck you. 
 * This is my kernel so I'll do it my way. Loading an entirely different process every time I want to load 
 * dynamic libraries is kinda dumb in my humble opinnion, since we can have the kernel do this for us
 * while also keeping things nice and lightweight
 */

// TODO: litterally everything

#include "dev/external.h"
#include "fs/file.h"
#include "libk/flow/error.h"

bool file_contains_drivers(file_t* file);

ErrorOrPtr load_external_driver(const char* path, extern_driver_t* out);

#endif // !__ANIVA_DRV_LOADER__

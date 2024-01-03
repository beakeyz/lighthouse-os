#include <stdio.h>
#include "defaults.h"
#include "lightos/proc/var_types.h"

struct profile_var_template base_defaults[] = {
  VAR_ENTRY("test", PROFILE_VAR_TYPE_STRING, "Yay"),
  VAR_ENTRY("test_2", PROFILE_VAR_TYPE_BYTE, VAR_NUM(4)),
  { },
};

struct profile_var_template global_defaults[] = {
  { },
};

/*!
 * @brief: Tool to create .pvr files for lightos 
 *
 * .pvr (dot profile variable) files are files that contain profile variables for a specific profile
 * They are used by the aniva kernel to read in saved profile variables for any profiles during system boot.
 * The kernel is able to create its own variable files, but we need some sort of bootstrap, which this tool
 * is for.
 *
 * NOTE: the name of the .pvr file should be the same as the profile name it targets. This means there can't be two 
 * files in the same directory, that target the same profile.
 *
 * Usage:
 * This tool can be used to create, modify, test, and install .pvr files. This is how it's used:
 *  base_env [target path] [flags] <flag args>
 * The target path is relative to the /system directory in the project root, which serves as an image for the ramdisk 
 * and the systems initial fs state.
 * Accepted flags are:
 *  -B : Create the default .pvr file for the BASE profile
 *  -G : Create the default .pvr file for the Global profile
 *  -e <VAR NAME> <new value> : Edits a variable with a value of the same type
 *  -r <VAR NAME> : Removes a variable
 *  -t <VAR NAME> <NEW TYPE> : Change the type of a variable
 *
 * File layout:
 * The layout of a .pvr file is closely related to how an ELF file is laid out. There is a header which defines some 
 */
int main(int argc, char** argv)
{
  printf("Welcome to the base_env manager\n");
}

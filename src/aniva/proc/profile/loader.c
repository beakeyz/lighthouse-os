
#include <libk/stddef.h>
#include "proc/profile/profile.h"

#define LT_PFB_MAGIC0 'P'
#define LT_PFB_MAGIC1 'F'
#define LT_PFB_MAGIC2 'b'
#define LT_PFB_MAGIC3 '\e'
#define LT_PFB_MAGIC "PFb\e"

/*
 * Structure of the file that contains a profile
 * this buffer starts at offset 0 inside the file
 *
 * When saving/loading this struct to/from a file, notice that we are making use of a
 * string table, just like in the ELF file format. This means that any
 * char* that we find in the file, are actually offsets into the string-
 * table
 */
struct lt_profile_buffer {
  char magic[4];
  /* Version of the kernel this was made for */
  uint32_t kernel_version;
  /* Offset from the start of the file to the start of the string table */
  uint32_t strtab_offset;
  proc_profile_t profile;
  profile_var_t vars[];
} __attribute__((packed));

/*!
 * @brief: Save @profile to a file
 *
 * Caller should ensure this file is usable and does not contain any sensitive data,
 * since we don't check the current contents of this file.
 * TODO: implement
 */
int profile_save(proc_profile_t* profile, file_t* file)
{
  return -1;
}

int profile_load(proc_profile_t** profile, file_t* file)
{
  return 0;
}

/*!
 * @brief: Save the variables of @profile
 *
 * TODO: implement
 */
int profile_save_variables(proc_profile_t* profile, file_t* file)
{
  return -1;
}

/*!
 * @brief: Load the variables in @file into @profiles variable store
 */
int profile_load_variables(proc_profile_t* profile, file_t* file)
{
  return 0;
}

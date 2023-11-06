
#include <libk/stddef.h>
#include "proc/profile/profile.h"

/*
 * Layout for the on-disk version of a proc profile
 * TODO:
 */
typedef struct proc_profile_file {
  uint8_t magic[2];
} __attribute__((packed)) proc_profile_file_t;

struct profile_file_var_entry {

};

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

#include <libk/stddef.h>
#include "libk/flow/error.h"
#include "proc/profile/profile.h"

#include <lightos/proc/var_types.h>

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
  kernel_panic("TODO: load variables");
  return 0;
}

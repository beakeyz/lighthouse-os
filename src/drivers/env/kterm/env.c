#include "lightos/proc/var_types.h"
#include "drivers/env/kterm/kterm.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "util.h"
#include <proc/profile/profile.h>
#include <proc/profile/variable.h>

/*!
 * @brief: Itterate function for the variables of a profile
 *
 * TODO: use @arg0 to communicate flags to the Itterate function
 */
static ErrorOrPtr print_profile_variable(void* _variable, uintptr_t arg0, uint64_t arg1)
{
  profile_var_t* var;

  if (!_variable)
    return Success(0);

  var = (profile_var_t*)_variable;

  if ((var->flags & PVAR_FLAG_HIDDEN) == PVAR_FLAG_HIDDEN)
    return Success(0);

  kterm_print(var->key);
  kterm_print(": ");

  switch (var->type) {
    case PROFILE_VAR_TYPE_STRING:
      kterm_println(var->str_value);
      break;
    default:
      kterm_println(to_string(var->qword_value));
  }

  return Success(0);
}

static ErrorOrPtr print_profiles(void* _profile, uintptr_t arg0, uint64_t arg1)
{
  proc_profile_t* profile;

  if (!_profile)
    return Success(0);

  profile = _profile;

  if (!profile->name)
    return Error();

  kterm_println(profile->name);

  return Success(0);
}

/*!
 * @brief: Print out info about the current profiles and/or their variables
 *
 * If a certain profile is specified, we print that profiles non-hidden variables
 * otherwise, we print all the available profiles
 */
uint32_t kterm_cmd_envinfo(const char** argv, size_t argc)
{
  int error;
  proc_profile_t* profile;

  profile = nullptr;

  switch (argc) {
    case 1:
      error = profile_foreach(print_profiles);

      if (error)
        return 3;

      break;
    case 2:
      error = profile_find(argv[1], &profile);

      if (error || !profile)
        return 2;

      /* Print every variable */
      hashmap_itterate(profile->var_map, print_profile_variable, NULL, NULL);
      break;
    default:
      return 1;
  }

  return 0;
}

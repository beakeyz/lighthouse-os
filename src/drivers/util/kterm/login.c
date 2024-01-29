#include "kterm.h"
#include "libk/io.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "proc/profile/profile.h"

/*!
 * @brief: Log into a profile
 *
 * This will make kterm act from the perspective of this profile
 * (It's current working directory wil become Root/Users/<profile name> for example)
 */
int kterm_do_login(proc_profile_t* profile)
{
  kterm_set_login(profile);
  return 0;
}

int kterm_find_target_profile(proc_profile_t** target)
{
  int error;
  proc_profile_t* l_target;
  const uint32_t prompt_size = 256;
  char* prompt_buffer = kmalloc(prompt_size);

  do {
    kterm_create_prompt("Select a profile to login: ", prompt_buffer, prompt_size, false);

    error = profile_find(prompt_buffer, &l_target);

    /* Invald? */
    if (error)
      continue;

    /* We can only log into default profiles */
    if (!profile_is_default(l_target)) {
      println("Can't log into that profile!");
      error = -1;
      continue;
    }

  } while (error);

  *target = l_target;
  kfree(prompt_buffer);
  return error;
}

static int kterm_prompt_password(proc_profile_t* target)
{
  const uint32_t prompt_size = 512;
  char* prompt_buffer = kmalloc(prompt_size);

  /* Prompt for a password */
  kterm_create_prompt("Please enter the passowrd: ", prompt_buffer, prompt_size, true);

  return profile_match_password(target, prompt_buffer);
}

/*!
 * @brief: Make sure the user gets logged in
 *
 * If there is no profile besides Global and BASE, prompt the creation of a new profile
 * TODO: Where do we prompt the user to create a new profile?
 */
int kterm_handle_login()
{
  int error;
  uint32_t passwd_retries;
  proc_profile_t* target_profile = NULL;

  error = kterm_find_target_profile(&target_profile);

  if (error)
    return error;

  /* If this profile has no password, we can simply login */
  if (!profile_has_password(target_profile))
    return kterm_do_login(target_profile);

  passwd_retries = 3;

  do {
    error = kterm_prompt_password(target_profile);

    if (!error)
      break;

    /* Funky delay */
    mdelay(2000);

    kterm_print("That was not quite correct. ");
    kterm_print(to_string(passwd_retries-1));
    kterm_println(" tries left!");

    passwd_retries--;
  } while (passwd_retries);

  if (error)
    return error;

  return kterm_do_login(target_profile);
}

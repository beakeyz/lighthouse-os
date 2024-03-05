#include "exec.h"
#include "dev/manifest.h"
#include "drivers/env/kterm/kterm.h"
#include "libk/bin/elf.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "logging/log.h"
#include "oss/core.h"
#include "oss/obj.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "proc/profile/profile.h"
#include "sched/scheduler.h"

/*!
 * @brief: Try to execute anything that is the first entry of @argv
 *
 * TODO: Search in respect to the PATH profile variable (Both of the global profile
 * and the currently logged in profile.)
 */
uint32_t kterm_try_exec(const char** argv, size_t argc)
{
  proc_id_t id;
  proc_t* p;
  const char* buffer;
  ErrorOrPtr result;
  proc_profile_t* login_profile;

  if (!argv || !argv[0])
    return 1;

  buffer = argv[0];

  if (buffer[0] == NULL)
    return 2;

  oss_obj_t* obj;

  if (oss_resolve_obj(buffer, &obj) || !obj) {
    logln("Could not find that object!");
    return 3;
  }

  /* Try to grab the file */
  file_t* file = oss_obj_get_file(obj);

  if (!file) {
    logln("Could not execute object!");

    oss_obj_close(obj);
    return 4;
  }

  /* NOTE: defer the schedule here, since we still need to attach a few handles to the process */
  result = elf_exec_64(file, false, true);

  oss_obj_close(file->m_obj);

  if (IsError(result)) {
    logln("Coult not execute object!");
    return 5;
  }

  id = (proc_id_t)Release(result);
  p = find_proc_by_id(id);

  /* Make sure the profile has the correct rights */
  kterm_get_login(&login_profile);
  proc_set_profile(p, login_profile);

  /*
   * Losing this process would be a fatal error because it compromises the entire system 
   * :clown: this design is peak stupidity
   */
  ASSERT_MSG(p, "Could not find process! (Lost to the void)");

  khandle_type_t driver_type = HNDL_TYPE_DRIVER;
  drv_manifest_t* kterm_manifest = get_driver("other/kterm");

  khandle_t _stdin;
  khandle_t _stdout;
  khandle_t _stderr;

  init_khandle(&_stdin, &driver_type, kterm_manifest);
  init_khandle(&_stdout, &driver_type, kterm_manifest);
  init_khandle(&_stderr, &driver_type, kterm_manifest);

  _stdin.flags |= HNDL_FLAG_READACCESS;
  _stdout.flags |= HNDL_FLAG_WRITEACCESS;
  _stderr.flags |= HNDL_FLAG_RW;

  bind_khandle(&p->m_handle_map, &_stdin);
  bind_khandle(&p->m_handle_map, &_stdout);
  bind_khandle(&p->m_handle_map, &_stderr);

  println("Adding to scheduler");
  /* Do an instant rescedule */
  Must(sched_add_priority_proc(p, true));

  println("Waiting for process...");
  ASSERT_MSG(await_proc_termination(id) == 0, "Process termination failed");
  println("Oinky");

  /* Make sure we're in terminal right after this exit */
  if (kterm_ismode(KTERM_MODE_GRAPHICS))
    kterm_switch_to_terminal();

  kterm_println(NULL);

  return 0;
}

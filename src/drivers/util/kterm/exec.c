#include "exec.h"
#include "dev/manifest.h"
#include "fs/vfs.h"
#include "fs/vobj.h"
#include "libk/bin/elf.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

uint32_t kterm_try_exec(const char** argv, size_t argc)
{
  proc_id_t id;
  proc_t* p;
  const char* buffer;
  ErrorOrPtr result;

  if (!argv || !argv[0])
    return 1;

  buffer = argv[0];

  if (buffer[0] == NULL)
    return 2;

  println("Resolve");
  vobj_t* obj = vfs_resolve(buffer);

  if (!obj)
    return 3;

  println("Get");
  file_t* file = vobj_get_file(obj);

  if (!file) {
    logln("Could not execute object!");
    return 4;
  }

  println("Create");
  result = elf_exec_static_64_ex(file, false, true);

  println("Close");
  vobj_close(obj);

  if (IsError(result)) {
    logln("Coult not execute object!");
    return 5;
  }

  id = (proc_id_t)Release(result);
  p = find_proc_by_id(id);

  /*
   * Losing this process would be a fatal error because it compromises the entire system 
   * :clown: this design is peak stupidity
   */
  ASSERT_MSG(p, "Could not find process! (Lost to the void)");

  khandle_type_t driver_type = HNDL_TYPE_DRIVER;
  dev_manifest_t* kterm_manifest = get_driver("other/kterm");

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

  /* Do an instant rescedule */
  sched_add_priority_proc(p, true);

  ASSERT_MSG(await_proc_termination(id) == 0, "Process termination failed");

  return 0;
}
